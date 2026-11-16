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
#include "ImGuiDrawDataCopy.h"
#include "Log.h"
#include "Material.h"
#include "Mailbox.h"
#include "Memory/LinearAllocator.h"
#include "Memory/MemoryUtil.h"
#include "Memory/RingAllocator.h"
#include "Memory/ThreadArenas.h"

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
  RenderObjectHandle hRenderObject;
};

struct DestroyRenderObjectMessage
{
  static constexpr MailboxMessageType TYPE = MailboxMessageType::DESTROY_RENDER_OBJECT;
  RenderObjectHandle hRenderObject;
};

struct SetRenderObjectMeshMessage
{
  static constexpr MailboxMessageType TYPE = MailboxMessageType::SET_RENDER_OBJECT_MESH;
  RenderObjectHandle hRenderObject;
  ResourceHandle hMesh;
};

struct SetRenderObjectMaterialMessage
{
  static constexpr MailboxMessageType TYPE = MailboxMessageType::SET_RENDER_OBJECT_MATERIAL;
  RenderObjectHandle hRenderObject;
  ResourceHandle hMaterial;
};

// Biggest property that fits in one message: a float4x4
static constexpr uint32_t MAX_MATERIAL_PROPERTY_SIZE = 64u;

// TODO: Fixed-size payload, a scalar still costs 64 bytes. Variable-size messages would fix it.
struct SetMaterialPropertyMessage
{
  static constexpr MailboxMessageType TYPE = MailboxMessageType::SET_MATERIAL_PROPERTY;
  ResourceHandle hMaterial;
  uint32_t uNameHash;
  uint32_t uSize;
  uint8_t aData[MAX_MATERIAL_PROPERTY_SIZE];
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

// Create requests (loader thread -> render thread). Each one is a single ring
// allocation holding the request and copies of the data it points to; the
// render thread frees it once the resource is created (or failed).
static constexpr size_t CREATE_REQUEST_RING_SIZE = 8u * 1024u * 1024u;

// Enough for any payload (vertex data, strings) and request struct
static constexpr size_t CREATE_REQUEST_ALIGNMENT = 16u;

// Guarded by g_oRenderMutex
static RingAllocator* g_pCreateRequestRing = nullptr;

// Loader thread waits here for room in the create request ring
static std::condition_variable g_oCreateRingSpaceCV;

// Guarded by g_oRenderMutex
static bool g_bRenderThreadExit = false;

static constexpr ThreadArenasConfig RENDER_THREAD_ARENAS_CONFIG = {
  /*zGlobalSize=*/ 1u * 1024u * 1024u,
  /*zSceneSize=*/  32u * 1024u * 1024u, // Meshes, materials and their API objects
  /*zFrameSize=*/  4u * 1024u * 1024u,  // Reset after every rendered frame
};

// Set by the window thread, consumed by the render thread
static std::atomic<bool> g_bResizePending = false;

// Render thread only
std::unordered_map<ResourceHandle, Mesh> g_dctMeshes;
std::unordered_map<ResourceHandle, Material*> g_dctMaterials;
std::unordered_map<RenderObjectHandle, RenderObject> g_dctRenderObjects;

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

// ImGui draw data travels next to each mailbox, in its own arena: it is too big for the
// message buffer. Owned like the mailbox itself: by the update thread while RECORDING
// (reset, then filled by SubmitImGuiDrawData), by the render thread while PROCESSING.
// The mailbox only goes back to IDLE once its frame is drawn.
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

static RenderObjectHandle GetNextHandle()
{
  // 0 is reserved as "no render object"
  static std::atomic<RenderObjectHandle> s_hNextHandle = 1;
  return s_hNextHandle.fetch_add(1, std::memory_order_relaxed);
}

// Update thread only. The handle is valid immediately; the render thread
// creates the object when it processes this frame's mailbox.
RenderObjectHandle CreateRenderObject(MailboxHandle _hMailbox)
{
  CreateRenderObjectMessage oMsg = {};
  oMsg.hRenderObject = GetNextHandle();
  if (!AddMessageToMailbox(_hMailbox, &oMsg))
  {
    return 0;
  }
  return oMsg.hRenderObject;
}

void DestroyRenderObject(MailboxHandle _hMailbox, RenderObjectHandle _hRenderObj)
{
  DestroyRenderObjectMessage oMsg = {};
  oMsg.hRenderObject = _hRenderObj;
  AddMessageToMailbox(_hMailbox, &oMsg);
}

void SetRenderObjectMesh(MailboxHandle _hMailbox, RenderObjectHandle _hRenderObj, ResourceHandle _hMesh)
{
  SetRenderObjectMeshMessage oMsg = {};
  oMsg.hRenderObject = _hRenderObj;
  oMsg.hMesh = _hMesh;
  AddMessageToMailbox(_hMailbox, &oMsg);
}

void SetRenderObjectMaterial(MailboxHandle _hMailbox, RenderObjectHandle _hRenderObj, ResourceHandle _hMaterial)
{
  SetRenderObjectMaterialMessage oMsg = {};
  oMsg.hRenderObject = _hRenderObj;
  oMsg.hMaterial = _hMaterial;
  AddMessageToMailbox(_hMailbox, &oMsg);
}

void SetMaterialProperty(MailboxHandle _hMailbox, ResourceHandle _hMaterial, const char* _sName, const void* _pData, uint32_t _uSize)
{
  ASSERT_MSG_FORMAT(_uSize <= MAX_MATERIAL_PROPERTY_SIZE, "Material property '%s' is %u bytes, max is %u", _sName, _uSize, MAX_MATERIAL_PROPERTY_SIZE);

  SetMaterialPropertyMessage oMsg = {};
  oMsg.hMaterial = _hMaterial;
  oMsg.uNameHash = HashName(_sName);
  oMsg.uSize = (_uSize < MAX_MATERIAL_PROPERTY_SIZE) ? _uSize : MAX_MATERIAL_PROPERTY_SIZE;
  memcpy(oMsg.aData, _pData, oMsg.uSize);
  AddMessageToMailbox(_hMailbox, &oMsg);
}

void SubmitImGuiDrawData(MailboxHandle _hMailbox, const ImDrawData* _pDrawData)
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

void RequestWindowResize()
{
  g_bResizePending = true;
}

//-----------------------------------------------------------------------------
// Resource creation requests (any thread -> render thread)
//-----------------------------------------------------------------------------

void InitRenderer(LinearAllocator* _pAllocator)
{
  char* pRingMemory = static_cast<char*>(_pAllocator->Allocate(CREATE_REQUEST_RING_SIZE, CREATE_REQUEST_ALIGNMENT));
  FATAL_ASSERT_MSG_FORMAT(pRingMemory, "Not enough memory for the create request ring (%zu bytes)", CREATE_REQUEST_RING_SIZE);
  RingAllocator* pRing = NewObject<RingAllocator>(_pAllocator, pRingMemory, CREATE_REQUEST_RING_SIZE);

  for (LinearAllocator*& rpArena : g_aImGuiArenas)
  {
    char* pArenaMemory = static_cast<char*>(_pAllocator->Allocate(IMGUI_DRAW_DATA_ARENA_SIZE, alignof(std::max_align_t)));
    FATAL_ASSERT_MSG_FORMAT(pArenaMemory, "Not enough memory for the ImGui draw data arenas (%zu bytes each)", IMGUI_DRAW_DATA_ARENA_SIZE);
    rpArena = NewObject<LinearAllocator>(_pAllocator, pArenaMemory, IMGUI_DRAW_DATA_ARENA_SIZE);
  }

  std::lock_guard<std::mutex> lock(g_oRenderMutex);
  g_pCreateRequestRing = pRing;
}

// Reserves room for one request, waiting for the render thread to free some if needed.
// Returns nullptr (logged) if it can never fit. Pair with PublishCreateRequest.
static char* AllocateCreateRequest(size_t _zSize)
{
  std::unique_lock<std::mutex> lock(g_oRenderMutex);

  FATAL_ASSERT_MSG(g_pCreateRequestRing, "InitRenderer was not called");
  if (!g_pCreateRequestRing->Fits(_zSize, CREATE_REQUEST_ALIGNMENT))
  {
    LogError("Create request of %zu bytes can never fit in the create request ring (%zu bytes): increase CREATE_REQUEST_RING_SIZE",
      _zSize, CREATE_REQUEST_RING_SIZE);
    return nullptr;
  }

  void* pMemory = nullptr;
  while ((pMemory = g_pCreateRequestRing->Allocate(_zSize, CREATE_REQUEST_ALIGNMENT)) == nullptr)
  {
    g_oCreateRingSpaceCV.wait(lock);
  }

  return static_cast<char*>(pMemory);
}

static void PublishCreateRequest()
{
  {
    std::lock_guard<std::mutex> lock(g_oRenderMutex);
    g_pCreateRequestRing->Publish();
  }
  g_oRenderCV.notify_all();
}

bool RequestCreateMesh(const MeshCreateRequest& _oRequest)
{
  PackedLayout oLayout;
  oLayout.Add(sizeof(MeshCreateRequest), alignof(MeshCreateRequest));
  const size_t zVertexOffset = oLayout.Add(_oRequest.zVertexDataSize, CREATE_REQUEST_ALIGNMENT);
  const size_t zIndexOffset = oLayout.Add(_oRequest.zIndexDataSize, CREATE_REQUEST_ALIGNMENT);

  char* pMemory = AllocateCreateRequest(oLayout.zSize);
  if (!pMemory)
  {
    return false;
  }

  if (_oRequest.zVertexDataSize > 0u)
  {
    memcpy(pMemory + zVertexOffset, _oRequest.pVertexData, _oRequest.zVertexDataSize);
  }
  if (_oRequest.zIndexDataSize > 0u)
  {
    memcpy(pMemory + zIndexOffset, _oRequest.pIndexData, _oRequest.zIndexDataSize);
  }

  MeshCreateRequest* pRequest = new (pMemory) MeshCreateRequest(_oRequest);
  pRequest->pVertexData = pMemory + zVertexOffset;
  pRequest->pIndexData = pMemory + zIndexOffset;

  PublishCreateRequest();
  return true;
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

bool RequestCreateMaterial(const MaterialCreateRequest& _oRequest)
{
  const RenderStateInfo& rInfo = _oRequest.oRenderStateInfo;

  PackedLayout oLayout;
  oLayout.Add(sizeof(MaterialCreateRequest), alignof(MaterialCreateRequest));
  const size_t zVSOffset = oLayout.Add(GetStringViewStorageSize(rInfo.m_sVSFilename), 1u);
  const size_t zPSOffset = oLayout.Add(GetStringViewStorageSize(rInfo.m_sPSFilename), 1u);
  const size_t zGSOffset = oLayout.Add(GetStringViewStorageSize(rInfo.m_sGSFilename), 1u);

  char* pMemory = AllocateCreateRequest(oLayout.zSize);
  if (!pMemory)
  {
    return false;
  }

  MaterialCreateRequest* pRequest = new (pMemory) MaterialCreateRequest(_oRequest);
  RenderStateInfo& rCopiedInfo = pRequest->oRenderStateInfo;
  rCopiedInfo.m_sVSFilename = CopyStringView(pMemory + zVSOffset, rInfo.m_sVSFilename);
  rCopiedInfo.m_sPSFilename = CopyStringView(pMemory + zPSOffset, rInfo.m_sPSFilename);
  rCopiedInfo.m_sGSFilename = CopyStringView(pMemory + zGSOffset, rInfo.m_sGSFilename);

  PublishCreateRequest();
  return true;
}

static bool ProcessCreateMesh(LinearAllocator* _pAllocator, const MeshCreateRequest* _pRequest)
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

  g_dctMeshes[_pRequest->hHandle] = oMesh;

  oRollback.Commit();
  return true;
}

static bool ProcessCreateMaterial(LinearAllocator* _pAllocator, const MaterialCreateRequest* _pRequest)
{
  RenderStateInfo oRSInfo = _pRequest->oRenderStateInfo;
  oRSInfo.m_uMeshConstantSize = sizeof(MeshConstants);

  // Properties start zeroed until the update thread sets them
  Material* pMaterial = CreateMaterial(_pAllocator, oRSInfo);
  if (!pMaterial)
  {
    return false;
  }

  g_dctMaterials[_pRequest->hHandle] = pMaterial;
  return true;
}

// Resources are created in _pAllocator (the render thread's scene arena).
// A bad asset (e.g. missing shader file) only fails that resource.
static void ProcessResourceCreateRequests(LinearAllocator* _pAllocator, const CreateRequestBase* _pRequest)
{
  bool bSucceeded = false;

  switch (_pRequest->eType)
  {
    case ResourceType::MESH:
      bSucceeded = ProcessCreateMesh(_pAllocator, static_cast<const MeshCreateRequest*>(_pRequest));
      break;
    case ResourceType::MATERIAL:
      bSucceeded = ProcessCreateMaterial(_pAllocator, static_cast<const MaterialCreateRequest*>(_pRequest));
      break;
    default:
      // TODO: Handle other resource types (e.g., TEXTURE)
      ASSERT_MSG_FORMAT(false, "Resource %u: unhandled resource type %d", _pRequest->hHandle, static_cast<int>(_pRequest->eType));
      break;
  }

  if (bSucceeded)
  {
    ResourceCompletedCallback(_pRequest->hHandle);
  }
  else
  {
    LogError("Resource %u: creation failed", _pRequest->hHandle);
    ResourceFailedCallback(_pRequest->hHandle);
  }
}

//-----------------------------------------------------------------------------
// Mailbox handoff
//-----------------------------------------------------------------------------

MailboxHandle AcquireMailboxWrite()
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

void ReleaseMailboxWrite(MailboxHandle _hMailbox)
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

static RenderObject* FindRenderObject(RenderObjectHandle _hRenderObj)
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
      auto it = g_dctMaterials.find(pMsg->hMaterial);
      if (it != g_dctMaterials.end())
      {
        // Unknown names are ignored: not every shader declares every property
        SetMaterialProperty(it->second, pMsg->uNameHash, pMsg->aData, pMsg->uSize);
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
      const Mesh* pMesh = nullptr;
      Material* pMaterial = nullptr;

      auto itMesh = g_dctMeshes.find(oPair.second.hMesh);
      if (itMesh != g_dctMeshes.end())
      {
        pMesh = &itMesh->second;
      }

      auto itMat = g_dctMaterials.find(oPair.second.hMaterial);
      if (itMat != g_dctMaterials.end())
      {
        pMaterial = itMat->second;
      }

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
    || g_pCreateRequestRing->Peek() != nullptr;
}

void RequestRenderThreadExit()
{
  {
    std::lock_guard<std::mutex> lock(g_oRenderMutex);
    g_bRenderThreadExit = true;
  }
  g_oRenderCV.notify_all();
}

// Render thread owns every GPU resource it created, so it destroys them before exiting.
// Requests still in the create ring are just dropped: the ring memory belongs to the main thread.
static void DestroyRenderResources(ThreadArenas* _pArenas)
{
  for (auto& oPair : g_dctMeshes)
  {
    api::DestroyMesh(oPair.second.pApiMesh);
  }
  g_dctMeshes.clear();

  for (auto& oPair : g_dctMaterials)
  {
    DestroyMaterial(oPair.second);
  }
  g_dctMaterials.clear();

  g_dctRenderObjects.clear();

  // Every object living in the scene arena has been destroyed above
  _pArenas->oScene.Reset();
}

void RenderThreadFnc()
{
  ThreadArenas* pArenas = CreateThreadArenas(RENDER_THREAD_ARENAS_CONFIG);

  while (true)
  {
    const CreateRequestBase* pRequest = nullptr;
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

      // Got create request? grab it (it stays in the ring until processed)
      pRequest = static_cast<const CreateRequestBase*>(g_pCreateRequestRing->Peek());

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

    if (pRequest)
    {
      ProcessResourceCreateRequests(&pArenas->oScene, pRequest);

      {
        std::lock_guard<std::mutex> lock(g_oRenderMutex);
        g_pCreateRequestRing->Free(const_cast<CreateRequestBase*>(pRequest));
      }
      // Wakes the loader thread if it is waiting for room in the ring
      g_oCreateRingSpaceCV.notify_one();
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
