#include "Renderer.h"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <unordered_map>

#include "imgui.h"

#include "API/GraphicsAPI.h"
#include "API/ImGuiRenderer.h"
#include "Asserts.h"
#include "Hash.h"
#include "GraphicsBackend/Vertex.h"
#include "ImGuiDrawDataCopy.h"
#include "Log.h"
#include "Material.h"
#include "Mailbox.h"
#include "Memory/LinearAllocator.h"
#include "Memory/MemoryUtil.h"
#include "Memory/ThreadArenas.h"
#include "MessageQueue.h"

struct MeshConstants
{
  float vMVP[16];
};

//-----------------------------------------------------------------------------
// Mailbox messages (update thread -> render thread)
//-----------------------------------------------------------------------------

enum class MailboxMessageType : uint32_t
{
  INVALID,
  CREATE_RENDER_OBJECT,
  DESTROY_RENDER_OBJECT,
  SET_RENDER_OBJECT_MESH,
  SET_RENDER_OBJECT_MATERIAL,
  SET_MATERIAL_PROPERTY,
  COUNT
};

struct CreateRenderObjectMessage
{
  static constexpr MailboxMessageType TYPE = MailboxMessageType::CREATE_RENDER_OBJECT;
  render::ObjectHandle hRenderObject;
};

struct DestroyRenderObjectMessage
{
  static constexpr MailboxMessageType TYPE = MailboxMessageType::DESTROY_RENDER_OBJECT;
  render::ObjectHandle hRenderObject;
};

struct SetRenderObjectMeshMessage
{
  static constexpr MailboxMessageType TYPE = MailboxMessageType::SET_RENDER_OBJECT_MESH;
  render::ObjectHandle hRenderObject;
  ResourceHandle hMesh;
};

struct SetRenderObjectMaterialMessage
{
  static constexpr MailboxMessageType TYPE = MailboxMessageType::SET_RENDER_OBJECT_MATERIAL;
  render::ObjectHandle hRenderObject;
  ResourceHandle hMaterial;
};

static constexpr uint32_t MAX_MATERIAL_PROPERTY_SIZE = 64u;

struct SetMaterialPropertyMessage
{
  static constexpr MailboxMessageType TYPE = MailboxMessageType::SET_MATERIAL_PROPERTY;
  ResourceHandle hMaterial;
  uint32_t uNameHash;
  uint32_t uSize;
  uint8_t aData[MAX_MATERIAL_PROPERTY_SIZE];
};

//-----------------------------------------------------------------------------
// Resource messages (loader thread -> render thread), sent through g_oCreateQueue
//-----------------------------------------------------------------------------

enum class ResourceMessageType : uint32_t
{
  INVALID,
  CREATE_MESH,
  CREATE_MATERIAL,
  RESOURCE_FAILED,
  UNLOAD_SCENE_RESOURCES,
  COUNT
};

// The data pointers point into the message's own extra bytes
struct CreateMeshMessage
{
  static constexpr ResourceMessageType TYPE = ResourceMessageType::CREATE_MESH;
  ResourceHandle hHandle;
  const void* pVertexData;
  size_t zVertexDataSize;
  const void* pIndexData;
  size_t zIndexDataSize;
};

// The filename views point into the message's own extra bytes
struct CreateMaterialMessage
{
  static constexpr ResourceMessageType TYPE = ResourceMessageType::CREATE_MATERIAL;
  ResourceHandle hHandle;
  RenderStateInfo oRenderStateInfo;
};

struct ResourceFailedMessage
{
  static constexpr ResourceMessageType TYPE = ResourceMessageType::RESOURCE_FAILED;
  ResourceHandle hHandle;
  ResourceType eResourceType;
};

// Everything queued before it belongs to the scene being unloaded
struct UnloadSceneResourcesMessage
{
  static constexpr ResourceMessageType TYPE = ResourceMessageType::UNLOAD_SCENE_RESOURCES;
};

//-----------------------------------------------------------------------------

struct Mesh
{
  api::Mesh* pApiMesh = nullptr;
  uint32_t uIndexCount = 0u;
};

struct RenderObject
{
  ResourceHandle hMesh = INVALID_RESOURCE_HANDLE;
  ResourceHandle hMaterial = INVALID_RESOURCE_HANDLE;
};

// Any external change that should make the render thread wake up
std::mutex g_oRenderMutex;
std::condition_variable g_oRenderCV;

// Resource messages (loader thread -> render thread). Each one carries copies of the data it
// points to; the render thread frees it once the resource is created (or failed).
// Shares g_oRenderMutex/g_oRenderCV so the render thread waits on frames and resources at once.
static constexpr size_t CREATE_QUEUE_SIZE = 8u * 1024u * 1024u;
static MessageQueue g_oCreateQueue;

// Guarded by g_oRenderMutex
static bool g_bRenderThreadExit = false;

static constexpr ThreadArenasConfig RENDER_THREAD_ARENAS_CONFIG = {
  /*zGlobalSize=*/ 1u * 1024u * 1024u,
  /*zSceneSize=*/  32u * 1024u * 1024u, // Meshes, materials and their API objects
  /*zFrameSize=*/  4u * 1024u * 1024u,  // Reset after every rendered frame
};

// Set by the window thread, consumed by the render thread
static std::atomic<bool> g_bResizePending = false;

// GPU side of a resource, at its handle's slot index
struct RenderResource
{
  ResourceHandle hHandle = INVALID_RESOURCE_HANDLE; // Set once created (or failed): lookups compare it
  ResourceType eType = ResourceType::INVALID;
  Mesh oMesh;
  Material* pMaterial = nullptr;
};

// Render thread only
static RenderResource g_aRenderResources[MAX_RESOURCES];
std::unordered_map<render::ObjectHandle, RenderObject> g_dctRenderObjects;

// Drawn in place of failed resources. Created in the render thread's global arena;
// a failed handle's RenderResource points to them. Null if they couldn't be created.
static Mesh g_oErrorMesh = {};
static Material* g_pErrorMaterial = nullptr;

static constexpr const char* ERROR_VS_FILENAME = "assets/engine/shaders/errorV.cso";
static constexpr const char* ERROR_PS_FILENAME = "assets/engine/shaders/errorP.cso";

//-----------------------------------------------------------------------------
// Mailboxes
//
// Update and render both alternate 0,1,0,1... so frames are always consumed
// in the order they were produced. Mailbox eState is guarded by g_oRenderMutex.
//-----------------------------------------------------------------------------

static constexpr size_t MAILBOX_CAPACITY = 64 * 1024;

alignas(16) static char g_aMailboxMemory0[MAILBOX_CAPACITY];
alignas(16) static char g_aMailboxMemory1[MAILBOX_CAPACITY];

static Mailbox g_oMailbox0 = { g_aMailboxMemory0, 0, MAILBOX_CAPACITY, MAILBOX_IDLE };
static Mailbox g_oMailbox1 = { g_aMailboxMemory1, 0, MAILBOX_CAPACITY, MAILBOX_IDLE };

static MailboxHandle g_hNextWriteMailbox = 0; // Update thread only
static MailboxHandle g_hNextReadMailbox = 0;  // Render thread only

static constexpr size_t IMGUI_DRAW_DATA_ARENA_SIZE = 2u * 1024u * 1024u;

static LinearAllocator* g_aImGuiArenas[2] = {};
static ImDrawData* g_aImGuiDrawData[2] = {};

Mailbox& GetMailbox(MailboxHandle _hMailbox)
{
  ASSERT_MSG_FORMAT(_hMailbox == 0 || _hMailbox == 1, "Invalid mailbox handle %u", _hMailbox);
  return (_hMailbox == 0) ? g_oMailbox0 : g_oMailbox1;
}

//-----------------------------------------------------------------------------
// Update thread API
//-----------------------------------------------------------------------------

static render::ObjectHandle GetNextHandle()
{
  // 0 is reserved as "no render object"
  static std::atomic<render::ObjectHandle> s_hNextHandle = 1;
  return s_hNextHandle.fetch_add(1, std::memory_order_relaxed);
}

// Update thread only. The handle is valid immediately; the render thread
// creates the object when it processes this frame's mailbox.
render::ObjectHandle render::CreateRenderObject(MailboxHandle _hMailbox)
{
  CreateRenderObjectMessage oMsg = {};
  oMsg.hRenderObject = GetNextHandle();
  if (!AddMessageToMailbox(_hMailbox, &oMsg))
  {
    return 0;
  }
  return oMsg.hRenderObject;
}

void render::DestroyRenderObject(MailboxHandle _hMailbox, render::ObjectHandle _hRenderObj)
{
  DestroyRenderObjectMessage oMsg = {};
  oMsg.hRenderObject = _hRenderObj;
  AddMessageToMailbox(_hMailbox, &oMsg);
}

void render::SetRenderObjectMesh(MailboxHandle _hMailbox, render::ObjectHandle _hRenderObj, ResourceHandle _hMesh)
{
  SetRenderObjectMeshMessage oMsg = {};
  oMsg.hRenderObject = _hRenderObj;
  oMsg.hMesh = _hMesh;
  AddMessageToMailbox(_hMailbox, &oMsg);
}

void render::SetRenderObjectMaterial(MailboxHandle _hMailbox, render::ObjectHandle _hRenderObj, ResourceHandle _hMaterial)
{
  SetRenderObjectMaterialMessage oMsg = {};
  oMsg.hRenderObject = _hRenderObj;
  oMsg.hMaterial = _hMaterial;
  AddMessageToMailbox(_hMailbox, &oMsg);
}

void render::SetMaterialProperty(MailboxHandle _hMailbox, ResourceHandle _hMaterial, const char* _sName, const void* _pData, uint32_t _uSize)
{
  ASSERT_MSG_FORMAT(_uSize <= MAX_MATERIAL_PROPERTY_SIZE, "Material property '%s' is %u bytes, max is %u", _sName, _uSize, MAX_MATERIAL_PROPERTY_SIZE);

  SetMaterialPropertyMessage oMsg = {};
  oMsg.hMaterial = _hMaterial;
  oMsg.uNameHash = HashName(_sName);
  oMsg.uSize = (_uSize < MAX_MATERIAL_PROPERTY_SIZE) ? _uSize : MAX_MATERIAL_PROPERTY_SIZE;
  memcpy(oMsg.aData, _pData, oMsg.uSize);
  AddMessageToMailbox(_hMailbox, &oMsg);
}

void render::SubmitImGuiDrawData(MailboxHandle _hMailbox, const ImDrawData* _pDrawData)
{
  ASSERT_MSG(g_aImGuiDrawData[_hMailbox] == nullptr, "SubmitImGuiDrawData called twice for the same mailbox");

  LinearAllocator* pArena = g_aImGuiArenas[_hMailbox];
  ImDrawData* pCopy = CopyImDrawData(pArena, _pDrawData);
  if (!pCopy)
  {
    // Losing the debug UI for a frame beats stopping the game, but it shouldn't go unnoticed
    LogError("ImGui draw data (%d vertices, %d indices) doesn't fit in %zu bytes: frame drawn without ImGui. Increase IMGUI_DRAW_DATA_ARENA_SIZE",
      _pDrawData->TotalVtxCount, _pDrawData->TotalIdxCount, IMGUI_DRAW_DATA_ARENA_SIZE);
    pArena->Reset();
    return;
  }

  g_aImGuiDrawData[_hMailbox] = pCopy;
}

void render::RequestWindowResize()
{
  g_bResizePending = true;
}

//-----------------------------------------------------------------------------
// Resource creation requests (any thread -> render thread)
//-----------------------------------------------------------------------------

void render::InitRenderer(LinearAllocator* _pAllocator)
{
  InitMessageQueue(&g_oCreateQueue, _pAllocator, CREATE_QUEUE_SIZE, &g_oRenderMutex, &g_oRenderCV);

  for (LinearAllocator*& rpArena : g_aImGuiArenas)
  {
    char* pArenaMemory = static_cast<char*>(_pAllocator->Allocate(IMGUI_DRAW_DATA_ARENA_SIZE, alignof(std::max_align_t)));
    FATAL_ASSERT_MSG_FORMAT(pArenaMemory, "Not enough memory for the ImGui draw data arenas (%zu bytes each)", IMGUI_DRAW_DATA_ARENA_SIZE);
    rpArena = NewObject<LinearAllocator>(_pAllocator, pArenaMemory, IMGUI_DRAW_DATA_ARENA_SIZE);
  }
}

bool render::RequestCreateMesh(ResourceHandle _hHandle, const void* _pVertexData, size_t _zVertexDataSize, const void* _pIndexData, size_t _zIndexDataSize)
{
  PackedLayout oLayout;
  const size_t zVertexOffset = oLayout.Add(_zVertexDataSize, MESSAGE_EXTRA_ALIGNMENT);
  const size_t zIndexOffset = oLayout.Add(_zIndexDataSize, MESSAGE_EXTRA_ALIGNMENT);

  char* pExtra = nullptr;
  CreateMeshMessage* pMsg = BeginQueueMessage<CreateMeshMessage>(&g_oCreateQueue, oLayout.zSize, &pExtra);
  if (!pMsg)
  {
    return false;
  }

  if (_zVertexDataSize > 0u)
  {
    memcpy(pExtra + zVertexOffset, _pVertexData, _zVertexDataSize);
  }
  if (_zIndexDataSize > 0u)
  {
    memcpy(pExtra + zIndexOffset, _pIndexData, _zIndexDataSize);
  }

  pMsg->hHandle = _hHandle;
  pMsg->pVertexData = pExtra + zVertexOffset;
  pMsg->pIndexData = pExtra + zIndexOffset;
  pMsg->zVertexDataSize = _zVertexDataSize;
  pMsg->zIndexDataSize = _zIndexDataSize;

  EndQueueMessage(&g_oCreateQueue);
  return true;
}

void render::RequestResourceFailed(ResourceHandle _hHandle, ResourceType _eType)
{
  ResourceFailedMessage* pMsg = BeginQueueMessage<ResourceFailedMessage>(&g_oCreateQueue);
  // Without it the handle would stay LOADING forever
  FATAL_ASSERT_MSG(pMsg, "Resource failed message doesn't fit in the create queue");

  pMsg->hHandle = _hHandle;
  pMsg->eResourceType = _eType;

  EndQueueMessage(&g_oCreateQueue);
}

void render::RequestUnloadScene()
{
  const bool bQueued = BeginQueueMessage<UnloadSceneResourcesMessage>(&g_oCreateQueue) != nullptr;
  FATAL_ASSERT_MSG(bQueued, "Unload scene message doesn't fit in the create queue");
  EndQueueMessage(&g_oCreateQueue);
}

// Copies _sString null-terminated to _pDest (sized with _sString.size() + 1) and points the view at the copy
static std::string_view CopyStringView(char* _pDest, std::string_view _sString)
{
  if (_sString.empty())
  {
    return {};
  }
  memcpy(_pDest, _sString.data(), _sString.size());
  _pDest[_sString.size()] = '\0';
  return std::string_view(_pDest, _sString.size());
}

static size_t GetStringViewStorageSize(std::string_view _sString)
{
  return _sString.empty() ? 0u : _sString.size() + 1u;
}

bool render::RequestCreateMaterial(ResourceHandle _hHandle, const RenderStateInfo& _oRenderStateInfo)
{
  PackedLayout oLayout;
  const size_t zVSOffset = oLayout.Add(GetStringViewStorageSize(_oRenderStateInfo.m_sVSFilename), 1u);
  const size_t zPSOffset = oLayout.Add(GetStringViewStorageSize(_oRenderStateInfo.m_sPSFilename), 1u);
  const size_t zGSOffset = oLayout.Add(GetStringViewStorageSize(_oRenderStateInfo.m_sGSFilename), 1u);

  char* pExtra = nullptr;
  CreateMaterialMessage* pMsg = BeginQueueMessage<CreateMaterialMessage>(&g_oCreateQueue, oLayout.zSize, &pExtra);
  if (!pMsg)
  {
    return false;
  }

  pMsg->hHandle = _hHandle;
  pMsg->oRenderStateInfo = _oRenderStateInfo;
  pMsg->oRenderStateInfo.m_sVSFilename = CopyStringView(pExtra + zVSOffset, _oRenderStateInfo.m_sVSFilename);
  pMsg->oRenderStateInfo.m_sPSFilename = CopyStringView(pExtra + zPSOffset, _oRenderStateInfo.m_sPSFilename);
  pMsg->oRenderStateInfo.m_sGSFilename = CopyStringView(pExtra + zGSOffset, _oRenderStateInfo.m_sGSFilename);

  EndQueueMessage(&g_oCreateQueue);
  return true;
}

static bool ProcessCreateMesh(LinearAllocator* _pAllocator, const CreateMeshMessage* _pRequest)
{
  // The backends bind the index buffer as 16-bit
  if (_pRequest->zIndexDataSize % sizeof(uint16_t) != 0u)
  {
    LogError("Resource %u: index data size %zu is not a multiple of 16-bit indices", _pRequest->hHandle, _pRequest->zIndexDataSize);
    return false;
  }

  // On failure the API releases what it created; oRollback hands its memory back too
  ScopedRollback oRollback(_pAllocator);

  Mesh oMesh = {};
  oMesh.pApiMesh = api::CreateMesh(_pAllocator, _pRequest->pVertexData, _pRequest->zVertexDataSize,
    _pRequest->pIndexData, _pRequest->zIndexDataSize, sizeof(MeshConstants));
  if (!oMesh.pApiMesh)
  {
    return false;
  }

  oMesh.uIndexCount = static_cast<uint32_t>(_pRequest->zIndexDataSize / sizeof(uint16_t));

  RenderResource& rResource = g_aRenderResources[GetResourceIndex(_pRequest->hHandle)];
  rResource.hHandle = _pRequest->hHandle;
  rResource.eType = ResourceType::MESH;
  rResource.oMesh = oMesh;

  oRollback.Commit();
  return true;
}

static bool ProcessCreateMaterial(LinearAllocator* _pAllocator, const CreateMaterialMessage* _pRequest)
{
  RenderStateInfo oRSInfo = _pRequest->oRenderStateInfo;
  oRSInfo.m_uMeshConstantSize = sizeof(MeshConstants);

  // Properties start zeroed until the update thread sets them
  Material* pMaterial = CreateMaterial(_pAllocator, oRSInfo);
  if (!pMaterial)
  {
    return false;
  }

  RenderResource& rResource = g_aRenderResources[GetResourceIndex(_pRequest->hHandle)];
  rResource.hHandle = _pRequest->hHandle;
  rResource.eType = ResourceType::MATERIAL;
  rResource.pMaterial = pMaterial;
  return true;
}

// From now on the handle draws with the error mesh/material
static void UseErrorResource(ResourceHandle _hHandle, ResourceType _eType)
{
  RenderResource& rResource = g_aRenderResources[GetResourceIndex(_hHandle)];
  rResource.hHandle = _hHandle;
  rResource.eType = _eType;
  rResource.oMesh = (_eType == ResourceType::MESH) ? g_oErrorMesh : Mesh{};
  rResource.pMaterial = (_eType == ResourceType::MATERIAL) ? g_pErrorMaterial : nullptr;
}

// Nullptr for stale handles, other types, and resources not created yet
static const Mesh* FindMesh(ResourceHandle _hHandle)
{
  const RenderResource& rResource = g_aRenderResources[GetResourceIndex(_hHandle)];
  const bool bFound = _hHandle != INVALID_RESOURCE_HANDLE && rResource.hHandle == _hHandle
    && rResource.eType == ResourceType::MESH && rResource.oMesh.pApiMesh;
  return bFound ? &rResource.oMesh : nullptr;
}

static Material* FindMaterial(ResourceHandle _hHandle)
{
  const RenderResource& rResource = g_aRenderResources[GetResourceIndex(_hHandle)];
  const bool bFound = _hHandle != INVALID_RESOURCE_HANDLE && rResource.hHandle == _hHandle
    && rResource.eType == ResourceType::MATERIAL;
  return bFound ? rResource.pMaterial : nullptr;
}

// Destroys the GPU objects (unless they are the shared error ones).
// The memory stays in the scene arena until it is reset.
static void DestroyRenderResource(RenderResource& rResource_)
{
  if (rResource_.oMesh.pApiMesh && rResource_.oMesh.pApiMesh != g_oErrorMesh.pApiMesh)
  {
    api::DestroyMesh(rResource_.oMesh.pApiMesh);
  }
  if (rResource_.pMaterial && rResource_.pMaterial != g_pErrorMaterial)
  {
    DestroyMaterial(rResource_.pMaterial);
  }
  rResource_ = {};
}

// Every resource request comes through the create queue before the unload marker that follows
// it, so they are all created (or failed) by now: nothing of the old scene is still in flight.
static void UnloadSceneResources(LinearAllocator* _pSceneArena)
{
  for (RenderResource& rResource : g_aRenderResources)
  {
    if (rResource.hHandle != INVALID_RESOURCE_HANDLE)
    {
      const ResourceHandle hHandle = rResource.hHandle;
      DestroyRenderResource(rResource);
      FreeResourceHandle(hHandle);
    }
  }

  // Every object living in the scene arena has been destroyed above
  _pSceneArena->Reset();
}

// Publishes the outcome of a create message to the update thread
static void FinishCreateResource(ResourceHandle _hHandle, ResourceType _eType, bool _bSucceeded)
{
  if (_bSucceeded)
  {
    ResourceCompletedCallback(_hHandle);
  }
  else
  {
    UseErrorResource(_hHandle, _eType);
    ResourceFailedCallback(_hHandle);
  }
}

static void ProcessResourceMessage(LinearAllocator* _pAllocator, const MessageHeader* _pHeader)
{
  switch (static_cast<ResourceMessageType>(_pHeader->uType))
  {
  case CreateMeshMessage::TYPE:
  {
    const CreateMeshMessage* pMsg = GetQueueMessagePayload<CreateMeshMessage>(_pHeader);
    FinishCreateResource(pMsg->hHandle, ResourceType::MESH, ProcessCreateMesh(_pAllocator, pMsg));
  }
  break;
  case CreateMaterialMessage::TYPE:
  {
    const CreateMaterialMessage* pMsg = GetQueueMessagePayload<CreateMaterialMessage>(_pHeader);
    FinishCreateResource(pMsg->hHandle, ResourceType::MATERIAL, ProcessCreateMaterial(_pAllocator, pMsg));
  }
  break;
  case ResourceFailedMessage::TYPE:
  {
    const ResourceFailedMessage* pMsg = GetQueueMessagePayload<ResourceFailedMessage>(_pHeader);
    FinishCreateResource(pMsg->hHandle, pMsg->eResourceType, false);
  }
  break;
  case UnloadSceneResourcesMessage::TYPE:
    UnloadSceneResources(_pAllocator);
    break;
  default:
    ASSERT_MSG_FORMAT(false, "Unknown resource message type %u", _pHeader->uType);
    break;
  }
}

//-----------------------------------------------------------------------------
// Mailbox handoff
//-----------------------------------------------------------------------------

MailboxHandle render::AcquireMailboxWrite()
{
  const MailboxHandle hHandle = g_hNextWriteMailbox;
  Mailbox& oMailbox = GetMailbox(hHandle);

  {
    std::unique_lock<std::mutex> lock(g_oRenderMutex);
    while (oMailbox.eState != MAILBOX_IDLE)
    {
      g_oRenderCV.wait(lock);
    }

    oMailbox.zOffset = 0;
    oMailbox.eState = MAILBOX_RECORDING;
  }

  // IDLE means the render thread has drawn this mailbox's frame: its ImGui copy is free
  g_aImGuiArenas[hHandle]->Reset();
  g_aImGuiDrawData[hHandle] = nullptr;

  g_hNextWriteMailbox = 1 - hHandle;
  return hHandle;
}

void render::ReleaseMailboxWrite(MailboxHandle _hMailbox)
{
  {
    std::lock_guard<std::mutex> lock(g_oRenderMutex);
    GetMailbox(_hMailbox).eState = MAILBOX_READY;
  }
  g_oRenderCV.notify_all();
}

static void ReleaseMailboxRead(MailboxHandle _hMailbox)
{
  {
    std::lock_guard<std::mutex> lock(g_oRenderMutex);
    GetMailbox(_hMailbox).eState = MAILBOX_IDLE;
  }
  // Wakes the update thread if it is waiting in AcquireMailboxWrite
  g_oRenderCV.notify_all();
}

//-----------------------------------------------------------------------------
// Render thread
//-----------------------------------------------------------------------------

static RenderObject* FindRenderObject(render::ObjectHandle _hRenderObj)
{
  auto it = g_dctRenderObjects.find(_hRenderObj);
  return (it != g_dctRenderObjects.end()) ? &it->second : nullptr;
}

void ProcessMailbox(Mailbox* _pMailbox)
{
  size_t offset = 0;
  const MailboxMessageHeader* pCurrHeader;
  const void* pCurrData = nullptr;
  while (PullMessageFromMailbox(_pMailbox, offset, &pCurrHeader, &pCurrData))
  {
    switch (static_cast<MailboxMessageType>(pCurrHeader->uType))
    {
    case CreateRenderObjectMessage::TYPE:
    {
      const CreateRenderObjectMessage* pMsg = reinterpret_cast<const CreateRenderObjectMessage*>(pCurrData);
      g_dctRenderObjects[pMsg->hRenderObject] = {};
    }
    break;
    case DestroyRenderObjectMessage::TYPE:
    {
      const DestroyRenderObjectMessage* pMsg = reinterpret_cast<const DestroyRenderObjectMessage*>(pCurrData);
      g_dctRenderObjects.erase(pMsg->hRenderObject);
    }
    break;
    case SetRenderObjectMeshMessage::TYPE:
    {
      const SetRenderObjectMeshMessage* pMsg = reinterpret_cast<const SetRenderObjectMeshMessage*>(pCurrData);
      if (RenderObject* pObj = FindRenderObject(pMsg->hRenderObject))
      {
        pObj->hMesh = pMsg->hMesh;
      }
    }
    break;
    case SetRenderObjectMaterialMessage::TYPE:
    {
      const SetRenderObjectMaterialMessage* pMsg = reinterpret_cast<const SetRenderObjectMaterialMessage*>(pCurrData);
      if (RenderObject* pObj = FindRenderObject(pMsg->hRenderObject))
      {
        pObj->hMaterial = pMsg->hMaterial;
      }
    }
    break;
    case SetMaterialPropertyMessage::TYPE:
    {
      const SetMaterialPropertyMessage* pMsg = reinterpret_cast<const SetMaterialPropertyMessage*>(pCurrData);
      if (Material* pMaterial = FindMaterial(pMsg->hMaterial))
      {
        // Unknown names are ignored: not every shader declares every property.
        // The error material is shared by every failed material, it never takes properties.
        if (pMaterial != g_pErrorMaterial)
        {
          SetMaterialProperty(pMaterial, pMsg->uNameHash, pMsg->aData, pMsg->uSize);
        }
      }
    }
    break;
    default:
      ASSERT_MSG_FORMAT(false, "Unknown mailbox message type %u", pCurrHeader->uType);
      break;
    }
  }
}

// _hMailbox is the frame being drawn (in PROCESSING): its ImGui draw data, if any, goes on top
void Render(MailboxHandle _hMailbox)
{
  if (g_bResizePending.exchange(false))
  {
    api::OnWindowResize();
  }

  // begin a frame, then bind state/mesh and draw
  if (api::BeginDraw() == 0)       // returns 1 when swapchain rebuild is required
  {
    api::ClearDefaultRenderTarget();

    api::BindDefaultRenderTarget();

    for (const auto& oPair : g_dctRenderObjects)
    {
      const Mesh* pMesh = FindMesh(oPair.second.hMesh);
      Material* pMaterial = FindMaterial(oPair.second.hMaterial);

      if (pMaterial && pMesh)
      {
        BindMaterial(pMaterial);

        MeshConstants constants = { /* compute MVP here */ };
        api::DrawMesh(pMaterial->pRenderState,
          pMesh->pApiMesh,
          pMesh->uIndexCount,
          &constants,
          sizeof(constants));
      }
    }

    if (ImDrawData* pImGuiDrawData = g_aImGuiDrawData[_hMailbox])
    {
      api::ImGuiRendererDraw(pImGuiDrawData);
    }

    api::UnbindDefaultRenderTarget();

    api::EndDraw();
    api::Present();
  }
}

// Called with g_oRenderMutex held
static bool HasWork()
{
  return g_bRenderThreadExit
    || GetMailbox(g_hNextReadMailbox).eState == MAILBOX_READY
    || PeekQueueMessageLocked(&g_oCreateQueue) != nullptr;
}

void render::RequestRenderThreadExit()
{
  {
    std::lock_guard<std::mutex> lock(g_oRenderMutex);
    g_bRenderThreadExit = true;
  }
  g_oRenderCV.notify_all();
}

static void CreateErrorResources(LinearAllocator* _pAllocator)
{
  static const Vertex s_aQuadVertices[4] = {
    { { -0.25f, -0.25f, 0.f } },
    { {  0.25f, -0.25f, 0.f } },
    { {  0.25f,  0.25f, 0.f } },
    { { -0.25f,  0.25f, 0.f } },
  };
  static const uint16_t s_aQuadIndices[6] = { 0, 1, 2, 0, 2, 3 };

  g_oErrorMesh.pApiMesh = api::CreateMesh(_pAllocator, s_aQuadVertices, sizeof(s_aQuadVertices),
    s_aQuadIndices, sizeof(s_aQuadIndices), sizeof(MeshConstants));
  g_oErrorMesh.uIndexCount = 6u;
  if (!g_oErrorMesh.pApiMesh)
  {
    LogError("Couldn't create the error mesh: failed meshes won't be drawn");
  }

  RenderStateInfo oRSInfo = {};
  oRSInfo.m_sVSFilename = ERROR_VS_FILENAME;
  oRSInfo.m_sPSFilename = ERROR_PS_FILENAME;
  oRSInfo.m_bDepthWrite = false;
  oRSInfo.m_bDepthRead = false;
  oRSInfo.m_eCullMode = CullMode::NONE;
  oRSInfo.m_eDepthCompareOp = DepthCompareOp::ALWAYS;
  oRSInfo.m_uMeshConstantSize = sizeof(MeshConstants);

  g_pErrorMaterial = CreateMaterial(_pAllocator, oRSInfo);
  if (!g_pErrorMaterial)
  {
    LogError("Couldn't create the error material: failed materials won't be drawn");
  }
}

static void DestroyErrorResources()
{
  if (g_oErrorMesh.pApiMesh)
  {
    api::DestroyMesh(g_oErrorMesh.pApiMesh);
  }
  g_oErrorMesh = {};

  if (g_pErrorMaterial)
  {
    DestroyMaterial(g_pErrorMaterial);
    g_pErrorMaterial = nullptr;
  }
}

// Render thread owns every GPU resource it created, so it destroys them before exiting.
// Messages still in the create queue are just dropped: the queue memory belongs to the main thread.
static void DestroyRenderResources(ThreadArenas* _pArenas)
{
  // Failed handles share the error resources, destroyed once after them
  UnloadSceneResources(&_pArenas->oScene);

  g_dctRenderObjects.clear();

  DestroyErrorResources();
}

void render::RenderThreadFnc()
{
  ThreadArenas* pArenas = CreateThreadArenas(RENDER_THREAD_ARENAS_CONFIG);

  CreateErrorResources(&pArenas->oGlobal);

  while (true)
  {
    const MessageHeader* pResourceMsg = nullptr;
    bool bHasFrame = false;
    MailboxHandle hReadMailbox = 0;

    {
      std::unique_lock<std::mutex> lock(g_oRenderMutex);
      while (!HasWork())
      {
        g_oRenderCV.wait(lock);
      }

      if (g_bRenderThreadExit)
      {
        break;
      }

      // Got a resource message? grab it (it stays in the queue until processed)
      pResourceMsg = PeekQueueMessageLocked(&g_oCreateQueue);

      // Next frame (in production order) ready? grab it
      Mailbox& oNext = GetMailbox(g_hNextReadMailbox);
      if (oNext.eState == MAILBOX_READY)
      {
        oNext.eState = MAILBOX_PROCESSING;
        hReadMailbox = g_hNextReadMailbox;
        g_hNextReadMailbox = 1 - g_hNextReadMailbox;
        bHasFrame = true;
      }
    }

    if (pResourceMsg)
    {
      ProcessResourceMessage(&pArenas->oScene, pResourceMsg);

      // Also wakes the loader thread if it is waiting for room in the queue
      FreeQueueMessage(&g_oCreateQueue, pResourceMsg);
    }

    if (bHasFrame)
    {
      ProcessMailbox(&GetMailbox(hReadMailbox));

      Render(hReadMailbox);

      // Messages are applied and the frame's ImGui copy is drawn, the mailbox can be reused.
      // Released after Render, not before: the update thread resets the ImGui copy on acquire.
      ReleaseMailboxRead(hReadMailbox);

      pArenas->oFrame.Reset();
    }
  }

  DestroyRenderResources(pArenas);

  DestroyThreadArenas(pArenas);
}
