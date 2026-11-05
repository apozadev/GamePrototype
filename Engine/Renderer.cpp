#include "Renderer.h"

#include <assert.h>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <unordered_map>

#include "API/GraphicsAPI.h"
#include "Hash.h"
#include "Material.h"
#include "Mailbox.h"

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

struct RenderObject
{
  ResourceHandle hMesh = INVALID_RESOURCE_HANDLE;
  ResourceHandle hMaterial = INVALID_RESOURCE_HANDLE;
};

// Any external change that should make the render thread wake up
std::mutex g_oRenderMutex;
std::condition_variable g_oRenderCV;

// Guarded by g_oRenderMutex. The queue owns the requests; the render thread deletes them.
std::queue<const CreateRequestBase*> g_qCreateRequestQueue;

// Guarded by g_oRenderMutex
static bool g_bRenderThreadExit = false;

// Set by the window thread, consumed by the render thread
static std::atomic<bool> g_bResizePending = false;

// Render thread only
std::unordered_map<ResourceHandle, api::Mesh*> g_dctMeshes;
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

Mailbox& GetMailbox(MailboxHandle _hMailbox)
{
  assert(_hMailbox == 0 || _hMailbox == 1);
  return (_hMailbox == 0) ? g_oMailbox0 : g_oMailbox1;
}

//-----------------------------------------------------------------------------
// Update thread API
//-----------------------------------------------------------------------------

RenderObjectHandle GetNextHandle()
{
  static RenderObjectHandle s_hNextHandle = 1;
  return s_hNextHandle++;
}

// Update thread only. The handle is valid immediately; the render thread
// creates the object when it processes this frame's mailbox.
RenderObjectHandle CreateRenderObject(MailboxHandle _hMailbox)
{
  CreateRenderObjectMessage oMsg = {};
  oMsg.hRenderObject = GetNextHandle();
  AddMessageToMailbox(_hMailbox, &oMsg);
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
  assert(_uSize <= MAX_MATERIAL_PROPERTY_SIZE);

  SetMaterialPropertyMessage oMsg = {};
  oMsg.hMaterial = _hMaterial;
  oMsg.uNameHash = HashName(_sName);
  oMsg.uSize = (_uSize < MAX_MATERIAL_PROPERTY_SIZE) ? _uSize : MAX_MATERIAL_PROPERTY_SIZE;
  memcpy(oMsg.aData, _pData, oMsg.uSize);
  AddMessageToMailbox(_hMailbox, &oMsg);
}

void RequestWindowResize()
{
  g_bResizePending = true;
}

//-----------------------------------------------------------------------------
// Resource creation requests (any thread -> render thread)
//-----------------------------------------------------------------------------

static void PushCreateRequest(const CreateRequestBase* _pRequest)
{
  {
    std::lock_guard<std::mutex> lock(g_oRenderMutex);
    g_qCreateRequestQueue.push(_pRequest);
  }
  g_oRenderCV.notify_one();
}

// The request struct is copied, but the vertex/index data it points to is not:
// it must stay alive until the resource is READY.
void RequestCreateMesh(const MeshCreateRequest& _oRequest)
{
  PushCreateRequest(new MeshCreateRequest(_oRequest));
}

void RequestCreateMaterial(const MaterialCreateRequest& _oRequest)
{
  PushCreateRequest(new MaterialCreateRequest(_oRequest));
}

void ProcessCreateMesh(const MeshCreateRequest* _pRequest)
{
  api::Mesh* pMesh = api::CreateMesh(_pRequest->pVertexData, _pRequest->zVertexDataSize,
    _pRequest->pIndexData, _pRequest->zIndexDataSize, sizeof(MeshConstants));

  g_dctMeshes[_pRequest->hHandle] = pMesh;
}

void ProcessCreateMaterial(const MaterialCreateRequest* _pRequest)
{
  RenderStateInfo oRSInfo = _pRequest->oRenderStateInfo;
  oRSInfo.m_uMeshConstantSize = sizeof(MeshConstants);

  // Properties start zeroed until the update thread sets them
  g_dctMaterials[_pRequest->hHandle] = CreateMaterial(oRSInfo);
}

static void DeleteCreateRequest(const CreateRequestBase* _pRequest)
{
  // Requests have no virtual destructor, so delete through the concrete type
  switch (_pRequest->eType)
  {
  case ResourceType::MESH:     delete static_cast<const MeshCreateRequest*>(_pRequest); break;
  case ResourceType::MATERIAL: delete static_cast<const MaterialCreateRequest*>(_pRequest); break;
  default: assert(false && "Unhandled create request type"); break;
  }
}

void ProcessResourceCreateRequests(const CreateRequestBase* _pRequest)
{
  switch (_pRequest->eType)
  {
    case ResourceType::MESH:
    {
      const MeshCreateRequest* pMeshRequest = static_cast<const MeshCreateRequest*>(_pRequest);
      ProcessCreateMesh(pMeshRequest);
    }
    break;
    case ResourceType::MATERIAL:
    {
      const MaterialCreateRequest* pMaterialRequest = static_cast<const MaterialCreateRequest*>(_pRequest);
      ProcessCreateMaterial(pMaterialRequest);
    }
    break;
    default:
      // TODO: Handle other resource types (e.g., TEXTURE)
      break;
  }

  ResourceCompletedCallback(_pRequest->hHandle);

  DeleteCreateRequest(_pRequest);
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
      assert(false && "Unknown mailbox message type");
      break;
    }
  }
}

void Render()
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
      api::Mesh* pMesh = nullptr;
      Material* pMaterial = nullptr;

      auto itMesh = g_dctMeshes.find(oPair.second.hMesh);
      if (itMesh != g_dctMeshes.end())
      {
        pMesh = itMesh->second;
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
          pMesh,
          /*indexCount=*/3, //TODO: create new Mesh struct containing api::Mesh and index count. Add index count to MeshCreateRequest.
          &constants,
          sizeof(constants));
      }
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
    || !g_qCreateRequestQueue.empty();
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
static void DestroyRenderResources()
{
  // Requests that never got processed
  {
    std::lock_guard<std::mutex> lock(g_oRenderMutex);
    while (!g_qCreateRequestQueue.empty())
    {
      DeleteCreateRequest(g_qCreateRequestQueue.front());
      g_qCreateRequestQueue.pop();
    }
  }

  for (auto& oPair : g_dctMeshes)
  {
    api::DestroyMesh(oPair.second);
  }
  g_dctMeshes.clear();

  for (auto& oPair : g_dctMaterials)
  {
    DestroyMaterial(oPair.second);
  }
  g_dctMaterials.clear();

  g_dctRenderObjects.clear();
}

void RenderThreadFnc()
{
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

      // Got create request? grab it
      if (!g_qCreateRequestQueue.empty())
      {
        pRequest = g_qCreateRequestQueue.front();
        g_qCreateRequestQueue.pop();
      }

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
      ProcessResourceCreateRequests(pRequest);
    }

    if (bHasFrame)
    {
      ProcessMailbox(&GetMailbox(hReadMailbox));

      // All messages are applied to render-thread state, the mailbox can be reused
      ReleaseMailboxRead(hReadMailbox);

      Render();
    }
  }

  DestroyRenderResources();
}
