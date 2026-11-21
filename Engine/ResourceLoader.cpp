#include "ResourceLoader.h"

#include <condition_variable>
#include <cstring>
#include <mutex>

#include "Asserts.h"
#include "Log.h"
#include "MessageQueue.h"
#include "ObjLoader.h"
#include "Renderer.h"
#include "GraphicsBackend/Vertex.h"
#include "Memory/LinearAllocator.h"
#include "Memory/MemoryUtil.h"
#include "Memory/ThreadArenas.h"
#include "Platform/File.h"

//-----------------------------------------------------------------------------
// Load messages (update thread -> loader thread), sent through s_oLoadQueue
//
// The strings a request info points to are copied into the message's extra
// bytes. Freed once the loader has processed it.
//-----------------------------------------------------------------------------

enum class LoadMessageType : uint32_t
{
  INVALID,
  LOAD_MESH,
  LOAD_MATERIAL,
  UNLOAD_SCENE,
  COUNT
};

struct LoadMeshMessage
{
  static constexpr LoadMessageType TYPE = LoadMessageType::LOAD_MESH;
  ResourceHandle hHandle;
  MeshLoadRequestInfo oInfo;
};

struct LoadMaterialMessage
{
  static constexpr LoadMessageType TYPE = LoadMessageType::LOAD_MATERIAL;
  ResourceHandle hHandle;
  MaterialLoadRequestInfo oInfo;
};

// Everything queued before it belongs to the scene being unloaded
struct UnloadSceneMessage
{
  static constexpr LoadMessageType TYPE = LoadMessageType::UNLOAD_SCENE;
};

static constexpr size_t LOAD_QUEUE_SIZE = 8u * 1024u * 1024u;

static constexpr ThreadArenasConfig LOADER_THREAD_ARENAS_CONFIG = {
  /*zGlobalSize=*/ 1u * 1024u * 1024u,
  /*zSceneSize=*/  1u * 1024u * 1024u,
  /*zFrameSize=*/  16u * 1024u * 1024u, // Reset after every message
};

static std::mutex oLoadRequestMutex;
static std::condition_variable oLoadRequestCV; // Loader waits for messages (or exit)

// Uses oLoadRequestMutex / oLoadRequestCV
static MessageQueue s_oLoadQueue;

// Guarded by oLoadRequestMutex
static bool bLoaderThreadExit = false;

static void ProcessLoadMessage(LinearAllocator* _pScratch, const MessageHeader* _pHeader);

//-----------------------------------------------------------------------------

void InitResourceLoader(LinearAllocator* _pAllocator)
{
  InitMessageQueue(&s_oLoadQueue, _pAllocator, LOAD_QUEUE_SIZE, &oLoadRequestMutex, &oLoadRequestCV);
}

// Starts a message in the load queue, waiting for the loader to free room if needed.
// A message that can never fit is fatal. Pair with EndQueueMessage.
template<typename T>
static T* BeginLoadMessage(size_t _zExtraSize = 0u, char** ppExtra_ = nullptr)
{
  T* pMsg = BeginQueueMessage<T>(&s_oLoadQueue, _zExtraSize, ppExtra_);
  FATAL_ASSERT_MSG_FORMAT(pMsg, "Load message with %zu extra bytes can never fit in the load queue (%zu bytes): increase LOAD_QUEUE_SIZE",
    _zExtraSize, LOAD_QUEUE_SIZE);
  return pMsg;
}

//-----------------------------------------------------------------------------

struct LoadGroup
{
  ResourceHandle* pResources;
  uint32_t uCount;
  uint32_t uCapacity;
};

LoadGroup* CreateLoadGroup(LinearAllocator* _pAllocator, uint32_t _uCapacity)
{
  LoadGroup* pGroup = NewObject<LoadGroup>(_pAllocator);
  pGroup->pResources = NewArray<ResourceHandle>(_pAllocator, _uCapacity);
  pGroup->uCapacity = _uCapacity;
  return pGroup;
}

// Allocates the handle of a new resource and registers it in its group
static ResourceHandle AddResourceToGroup(LoadGroup* _pGroup)
{
  FATAL_ASSERT_MSG(_pGroup, "Resources must be loaded into a load group");
  FATAL_ASSERT_MSG_FORMAT(_pGroup->uCount < _pGroup->uCapacity, "Load group is full (%u resources)", _pGroup->uCapacity);

  const ResourceHandle hHandle = AllocateResourceHandle();
  _pGroup->pResources[_pGroup->uCount++] = hHandle;
  return hHandle;
}

LoadGroupStatus GetLoadGroupStatus(const LoadGroup* _pGroup)
{
  LoadGroupStatus oStatus = {};
  oStatus.uTotal = _pGroup->uCount;

  for (uint32_t i = 0u; i < _pGroup->uCount; i++)
  {
    switch (GetResourceState(_pGroup->pResources[i]))
    {
      case ResourceState::READY:
        oStatus.uReady++;
        break;
      case ResourceState::FAILED:
      case ResourceState::INVALID:
        oStatus.uFailed++;
        break;
      default:
        break;
    }
  }

  if (oStatus.uReady + oStatus.uFailed < oStatus.uTotal)
  {
    oStatus.eState = LoadGroupState::LOADING;
  }
  else
  {
    oStatus.eState = (oStatus.uFailed > 0u) ? LoadGroupState::FAILED : LoadGroupState::READY;
  }
  return oStatus;
}

void UnloadScene()
{
  BeginLoadMessage<UnloadSceneMessage>();
  EndQueueMessage(&s_oLoadQueue);
}

//-----------------------------------------------------------------------------

static size_t GetStringStorageSize(const char* _sString)
{
  return _sString ? strlen(_sString) + 1u : 0u;
}

// _sString must fit in the zSize bytes reserved with GetStringStorageSize
static const char* CopyString(char* _pDest, const char* _sString)
{
  if (!_sString)
  {
    return nullptr;
  }
  memcpy(_pDest, _sString, strlen(_sString) + 1u);
  return _pDest;
}

ResourceHandle LoadResourceAsync(LoadGroup* _pGroup, const MeshLoadRequestInfo& _oInfo)
{
  const ResourceHandle hHandle = AddResourceToGroup(_pGroup);

  char* pExtra = nullptr;
  LoadMeshMessage* pMsg = BeginLoadMessage<LoadMeshMessage>(GetStringStorageSize(_oInfo.pFilename), &pExtra);
  pMsg->hHandle = hHandle;
  pMsg->oInfo = _oInfo;
  pMsg->oInfo.pFilename = CopyString(pExtra, _oInfo.pFilename);
  EndQueueMessage(&s_oLoadQueue);

  return hHandle;
}

ResourceHandle LoadResourceAsync(LoadGroup* _pGroup, const MaterialLoadRequestInfo& _oInfo)
{
  const ResourceHandle hHandle = AddResourceToGroup(_pGroup);

  PackedLayout oLayout;
  const size_t zVSOffset = oLayout.Add(GetStringStorageSize(_oInfo.pVSFilename), 1u);
  const size_t zPSOffset = oLayout.Add(GetStringStorageSize(_oInfo.pPSFilename), 1u);

  char* pExtra = nullptr;
  LoadMaterialMessage* pMsg = BeginLoadMessage<LoadMaterialMessage>(oLayout.zSize, &pExtra);
  pMsg->hHandle = hHandle;
  pMsg->oInfo = _oInfo;
  pMsg->oInfo.pVSFilename = CopyString(pExtra + zVSOffset, _oInfo.pVSFilename);
  pMsg->oInfo.pPSFilename = CopyString(pExtra + zPSOffset, _oInfo.pPSFilename);
  EndQueueMessage(&s_oLoadQueue);

  return hHandle;
}

//-----------------------------------------------------------------------------

void RequestLoaderThreadExit()
{
  {
    std::lock_guard<std::mutex> lock(oLoadRequestMutex);
    bLoaderThreadExit = true;
  }
  oLoadRequestCV.notify_all();
}

//-----------------------------------------------------------------------------

void WorkerThreadProcessLoadRequests()
{
  ThreadArenas* pArenas = CreateThreadArenas(LOADER_THREAD_ARENAS_CONFIG);

  while (true)
  {
    const MessageHeader* pHeader = nullptr;

    {
      // Wait for load messages to be queued
      std::unique_lock<std::mutex> lock(oLoadRequestMutex);
      while (!bLoaderThreadExit && (pHeader = PeekQueueMessageLocked(&s_oLoadQueue)) == nullptr)
      {
        oLoadRequestCV.wait(lock);
      }

      if (bLoaderThreadExit)
      {
        break;
      }
    }

    ProcessLoadMessage(&pArenas->oFrame, pHeader);

    // Whatever happened, the message (and its copied data) is done with.
    // Also wakes the update thread if it is waiting for room in the queue.
    FreeQueueMessage(&s_oLoadQueue, pHeader);

    pArenas->oFrame.Reset();
  }

  DestroyThreadArenas(pArenas);
}

//-----------------------------------------------------------------------------

// File contents and the built geometry live in _pScratch until they're copied into the create message
static bool LoadMesh(LinearAllocator* _pScratch, ResourceHandle _hHandle, const MeshLoadRequestInfo& _oInfo)
{
  if (!_oInfo.pFilename)
  {
    LogError("Resource %u: mesh without a filename", _hHandle);
    return false;
  }

  size_t zFileSize = 0u;
  const char* pText = static_cast<const char*>(platform::ReadFile(_oInfo.pFilename, _pScratch, zFileSize));
  if (!pText)
  {
    return false;
  }

  ObjMesh oMesh;
  if (!ParseObj(pText, zFileSize, _pScratch, oMesh))
  {
    LogError("Resource %u: failed to load %s", _hHandle, _oInfo.pFilename);
    return false;
  }

  return render::RequestCreateMesh(_hHandle, oMesh.pVertices, oMesh.uVertexCount * sizeof(Vertex), oMesh.pIndices, oMesh.uIndexCount * sizeof(uint16_t));
}

static bool LoadMaterial(ResourceHandle _hHandle, const MaterialLoadRequestInfo& _oRequestInfo)
{
  RenderStateInfo oRenderStateInfo = {};
  oRenderStateInfo.m_sVSFilename = _oRequestInfo.pVSFilename ? _oRequestInfo.pVSFilename : "";
  oRenderStateInfo.m_sPSFilename = _oRequestInfo.pPSFilename ? _oRequestInfo.pPSFilename : "";
  oRenderStateInfo.m_bBlendEnabled = _oRequestInfo.bBlendEnabled;
  oRenderStateInfo.m_bDepthWrite = false;
  oRenderStateInfo.m_bDepthRead = false;
  oRenderStateInfo.m_eCullMode = CullMode::NONE;
  oRenderStateInfo.m_eDepthCompareOp = DepthCompareOp::ALWAYS;

  return render::RequestCreateMaterial(_hHandle, oRenderStateInfo);
}

static void ProcessLoadMessage(LinearAllocator* _pScratch, const MessageHeader* _pHeader)
{
  switch (static_cast<LoadMessageType>(_pHeader->uType))
  {
  case LoadMeshMessage::TYPE:
  {
    const LoadMeshMessage* pMsg = GetQueueMessagePayload<LoadMeshMessage>(_pHeader);
    if (!LoadMesh(_pScratch, pMsg->hHandle, pMsg->oInfo))
    {
      render::RequestResourceFailed(pMsg->hHandle, ResourceType::MESH);
    }
  }
  break;
  case LoadMaterialMessage::TYPE:
  {
    const LoadMaterialMessage* pMsg = GetQueueMessagePayload<LoadMaterialMessage>(_pHeader);
    if (!LoadMaterial(pMsg->hHandle, pMsg->oInfo))
    {
      render::RequestResourceFailed(pMsg->hHandle, ResourceType::MATERIAL);
    }
  }
  break;
  case UnloadSceneMessage::TYPE:
    // Passed on in order: the render thread gets it after every message of the scene being unloaded
    render::RequestUnloadScene();
    break;
  default:
    ASSERT_MSG_FORMAT(false, "Unknown load message type %u", _pHeader->uType);
    break;
  }
}
