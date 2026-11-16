#include "ResourceLoader.h"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <new>
#include <unordered_map>

#include "Asserts.h"
#include "Log.h"
#include "ObjLoader.h"
#include "Renderer.h"
#include "GraphicsBackend/Vertex.h"
#include "Memory/LinearAllocator.h"
#include "Memory/MemoryUtil.h"
#include "Memory/RingAllocator.h"
#include "Memory/ThreadArenas.h"
#include "Platform/File.h"

//-----------------------------------------------------------------------------
// Load requests (update thread -> loader thread)
//
// Each request is one ring allocation: the request struct followed by copies of
// the data it points to. Freed once the loader has processed it.
//-----------------------------------------------------------------------------

struct ResourceLoadRequestBase
{
  ResourceHandle hHandle;
  ResourceType eType;
};

template<typename T>
struct ResourceLoadRequest : public ResourceLoadRequestBase
{
  T oRequestInfo;
};

struct ResourceEntry
{
  ResourceType eType;
  ResourceState eState;
};

// Enough for any payload (vertex data, strings) and request struct
static constexpr size_t REQUEST_ALIGNMENT = 16u;

static constexpr size_t LOAD_REQUEST_RING_SIZE = 8u * 1024u * 1024u;

static constexpr ThreadArenasConfig LOADER_THREAD_ARENAS_CONFIG = {
  /*zGlobalSize=*/ 1u * 1024u * 1024u,
  /*zSceneSize=*/  1u * 1024u * 1024u,
  /*zFrameSize=*/  16u * 1024u * 1024u, // Reset after every request
};

static std::mutex oLoadRequestMutex;
static std::condition_variable oLoadRequestCV;   // Loader waits for requests
static std::condition_variable oLoadRingSpaceCV; // Update thread waits for room in the ring

// Guarded by oLoadRequestMutex
static RingAllocator* pLoadRequestRing = nullptr;
static bool bLoaderThreadExit = false;

static std::mutex oResourceRegistryMutex;

static std::unordered_map<ResourceHandle, ResourceEntry> dctResourceRegistry;

static void ProcessLoadRequest(LinearAllocator* _pScratch, const ResourceLoadRequestBase* _pRequest);

//-----------------------------------------------------------------------------

void InitResourceLoader(LinearAllocator* _pAllocator)
{
  char* pRingMemory = static_cast<char*>(_pAllocator->Allocate(LOAD_REQUEST_RING_SIZE, REQUEST_ALIGNMENT));
  FATAL_ASSERT_MSG_FORMAT(pRingMemory, "Not enough memory for the load request ring (%zu bytes)", LOAD_REQUEST_RING_SIZE);
  RingAllocator* pRing = NewObject<RingAllocator>(_pAllocator, pRingMemory, LOAD_REQUEST_RING_SIZE);

  std::lock_guard<std::mutex> lock(oLoadRequestMutex);
  pLoadRequestRing = pRing;
}

//-----------------------------------------------------------------------------

static void SetResourceState(ResourceHandle _hHandle, ResourceState _eState)
{
  std::lock_guard<std::mutex> lock(oResourceRegistryMutex);

  auto it = dctResourceRegistry.find(_hHandle);
  if (it != dctResourceRegistry.end())
  {
    it->second.eState = _eState;
  }
}

void ResourceCompletedCallback(ResourceHandle _hHandle)
{
  SetResourceState(_hHandle, ResourceState::READY);
}

void ResourceFailedCallback(ResourceHandle _hHandle)
{
  SetResourceState(_hHandle, ResourceState::FAILED);
}

//-----------------------------------------------------------------------------

static ResourceHandle GetNextResourceHandle()
{
  // 0 is INVALID_RESOURCE_HANDLE
  static std::atomic<ResourceHandle> s_hNextResourceHandle = 1;
  return s_hNextResourceHandle.fetch_add(1, std::memory_order_relaxed);
}

//-----------------------------------------------------------------------------

// Reserves room for one request in the ring, waiting for the loader to free some if needed.
// Pair with PublishLoadRequest.
static char* AllocateLoadRequest(size_t _zSize)
{
  std::unique_lock<std::mutex> lock(oLoadRequestMutex);

  FATAL_ASSERT_MSG(pLoadRequestRing, "InitResourceLoader was not called");
  FATAL_ASSERT_MSG_FORMAT(pLoadRequestRing->Fits(_zSize, REQUEST_ALIGNMENT),
    "Load request of %zu bytes can never fit in the load request ring (%zu bytes): increase LOAD_REQUEST_RING_SIZE",
    _zSize, LOAD_REQUEST_RING_SIZE);

  void* pMemory = nullptr;
  while ((pMemory = pLoadRequestRing->Allocate(_zSize, REQUEST_ALIGNMENT)) == nullptr)
  {
    oLoadRingSpaceCV.wait(lock);
  }

  return static_cast<char*>(pMemory);
}

// Registers the resource and hands the (fully written) request to the loader
static ResourceHandle PublishLoadRequest(ResourceLoadRequestBase* _pRequest, ResourceType _eType)
{
  ResourceHandle hHandle = GetNextResourceHandle();

  {
    std::lock_guard<std::mutex> lock(oResourceRegistryMutex);
    dctResourceRegistry.insert({ hHandle, { _eType, ResourceState::LOADING } });
  }

  _pRequest->hHandle = hHandle;
  _pRequest->eType = _eType;

  {
    std::lock_guard<std::mutex> lock(oLoadRequestMutex);
    pLoadRequestRing->Publish();
  }
  oLoadRequestCV.notify_one();

  return hHandle;
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

ResourceHandle LoadResourceAsync(const MeshLoadRequestInfo& _oInfo)
{
  using Request_t = ResourceLoadRequest<MeshLoadRequestInfo>;

  PackedLayout oLayout;
  oLayout.Add(sizeof(Request_t), alignof(Request_t));
  const size_t zFilenameOffset = oLayout.Add(GetStringStorageSize(_oInfo.pFilename), 1u);

  char* pMemory = AllocateLoadRequest(oLayout.zSize);

  Request_t* pRequest = new (pMemory) Request_t();
  pRequest->oRequestInfo = _oInfo;
  pRequest->oRequestInfo.pFilename = CopyString(pMemory + zFilenameOffset, _oInfo.pFilename);

  return PublishLoadRequest(pRequest, ResourceType::MESH);
}

ResourceHandle LoadResourceAsync(const MaterialLoadRequestInfo& _oInfo)
{
  using Request_t = ResourceLoadRequest<MaterialLoadRequestInfo>;

  PackedLayout oLayout;
  oLayout.Add(sizeof(Request_t), alignof(Request_t));
  const size_t zVSOffset = oLayout.Add(GetStringStorageSize(_oInfo.pVSFilename), 1u);
  const size_t zPSOffset = oLayout.Add(GetStringStorageSize(_oInfo.pPSFilename), 1u);

  char* pMemory = AllocateLoadRequest(oLayout.zSize);

  Request_t* pRequest = new (pMemory) Request_t();
  pRequest->oRequestInfo = _oInfo;
  pRequest->oRequestInfo.pVSFilename = CopyString(pMemory + zVSOffset, _oInfo.pVSFilename);
  pRequest->oRequestInfo.pPSFilename = CopyString(pMemory + zPSOffset, _oInfo.pPSFilename);

  return PublishLoadRequest(pRequest, ResourceType::MATERIAL);
}

//-----------------------------------------------------------------------------

ResourceState GetResourceState(ResourceHandle _hHandle)
{
  std::lock_guard<std::mutex> lock(oResourceRegistryMutex);

  auto it = dctResourceRegistry.find(_hHandle);
  if (it != dctResourceRegistry.end())
  {
    return it->second.eState;
  }

  return ResourceState::INVALID;
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
    const ResourceLoadRequestBase* pRequest = nullptr;

    {
      // Wait for load requests to be queued
      std::unique_lock<std::mutex> lock(oLoadRequestMutex);
      while (!bLoaderThreadExit && (pRequest = static_cast<const ResourceLoadRequestBase*>(pLoadRequestRing->Peek())) == nullptr)
      {
        oLoadRequestCV.wait(lock);
      }

      if (bLoaderThreadExit)
      {
        break;
      }
    }

    ProcessLoadRequest(&pArenas->oFrame, pRequest);

    // Whatever happened, the request (and its copied data) is done with
    {
      std::lock_guard<std::mutex> lock(oLoadRequestMutex);
      pLoadRequestRing->Free(const_cast<ResourceLoadRequestBase*>(pRequest));
    }
    oLoadRingSpaceCV.notify_one();

    pArenas->oFrame.Reset();
  }

  DestroyThreadArenas(pArenas);
}

//-----------------------------------------------------------------------------

// File contents and the built geometry live in _pScratch until they're copied into the create request
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

  // The render thread will call ResourceCompletedCallback once the GPU mesh is created
  MeshCreateRequest oCreateRequest = {};
  oCreateRequest.hHandle = _hHandle;
  oCreateRequest.eType = ResourceType::MESH;
  oCreateRequest.pVertexData = oMesh.pVertices;
  oCreateRequest.zVertexDataSize = oMesh.uVertexCount * sizeof(Vertex);
  oCreateRequest.pIndexData = oMesh.pIndices;
  oCreateRequest.zIndexDataSize = oMesh.uIndexCount * sizeof(uint16_t);
  return RequestCreateMesh(oCreateRequest);
}

static bool LoadMaterial(ResourceHandle _hHandle, const MaterialLoadRequestInfo& _oInfo)
{
  // The render thread will call ResourceCompletedCallback once the pipeline, descriptors, etc. are created
  MaterialCreateRequest oCreateRequest = {};
  oCreateRequest.hHandle = _hHandle;
  oCreateRequest.eType = ResourceType::MATERIAL;

  RenderStateInfo& rRSInfo = oCreateRequest.oRenderStateInfo;
  rRSInfo.m_sVSFilename = _oInfo.pVSFilename ? _oInfo.pVSFilename : "";
  rRSInfo.m_sPSFilename = _oInfo.pPSFilename ? _oInfo.pPSFilename : "";
  rRSInfo.m_bBlendEnabled = _oInfo.bBlendEnabled;
  rRSInfo.m_bDepthWrite = false;
  rRSInfo.m_bDepthRead = false;
  rRSInfo.m_eCullMode = CullMode::NONE;
  rRSInfo.m_eDepthCompareOp = DepthCompareOp::ALWAYS;

  return RequestCreateMaterial(oCreateRequest);
}

static void ProcessLoadRequest(LinearAllocator* _pScratch, const ResourceLoadRequestBase* _pRequest)
{
  bool bSucceeded = false;

  switch (_pRequest->eType)
  {
  case ResourceType::MESH:
    bSucceeded = LoadMesh(_pScratch, _pRequest->hHandle, static_cast<const ResourceLoadRequest<MeshLoadRequestInfo>*>(_pRequest)->oRequestInfo);
    break;
  case ResourceType::MATERIAL:
    bSucceeded = LoadMaterial(_pRequest->hHandle, static_cast<const ResourceLoadRequest<MaterialLoadRequestInfo>*>(_pRequest)->oRequestInfo);
    break;
  default:
    ASSERT_MSG_FORMAT(false, "Resource %u: unhandled load request type %d", _pRequest->hHandle, static_cast<int>(_pRequest->eType));
    break;
  }

  if (!bSucceeded)
  {
    ResourceFailedCallback(_pRequest->hHandle);
  }
}
