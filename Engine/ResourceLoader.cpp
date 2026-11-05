#include "ResourceLoader.h"

#include "Renderer.h"

//-----------------------------------------------------------------------------

static std::condition_variable oLoadRequestCV;
static std::mutex oLoadRequestMutex;

static std::queue<ResourceLoadRequestBase*> qRequestQueue;
static std::queue<ResourceHandle> qResourceCompleteQueue;

static std::mutex oResourceRegistryMutex;

static std::unordered_map<ResourceHandle, ResourceEntry> dctResourceRegistry;

// Guarded by oLoadRequestMutex
static bool bLoaderThreadExit = false;

static void ProcessLoadRequest(ResourceLoadRequestBase* _pRequest);

//-----------------------------------------------------------------------------

void ResourceCompletedCallback(ResourceHandle _hHandle)
{
  std::lock_guard<std::mutex> lock(oResourceRegistryMutex);

  auto it = dctResourceRegistry.find(_hHandle);
  if (it != dctResourceRegistry.end())
  {
    it->second.eState = ResourceState::READY;
  }
}

//-----------------------------------------------------------------------------

ResourceHandle GetNextResourceHandle()
{
  // 0 is INVALID_RESOURCE_HANDLE
  static ResourceHandle hNextResourceHandle = 1;
  return hNextResourceHandle++;
}

//-----------------------------------------------------------------------------

ResourceHandle LoadResourceAsyncInternal(ResourceLoadRequestBase* _pRequest, ResourceType _eType)
{
  ResourceHandle hHandle = GetNextResourceHandle();

  {
    std::lock_guard<std::mutex> lock(oResourceRegistryMutex);
    dctResourceRegistry.insert({ hHandle, { _eType, ResourceState::LOADING } });
  }

  _pRequest->hHandle = hHandle;
  {
    std::lock_guard<std::mutex> lock(oLoadRequestMutex);
    qRequestQueue.push(_pRequest);
  }
  oLoadRequestCV.notify_one();

  return hHandle;
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
  while (true)
  {
    ResourceLoadRequestBase* pRequest = nullptr;

    {
      // Wait for load requests to be queued
      std::unique_lock<std::mutex> lock(oLoadRequestMutex);
      while (qRequestQueue.empty() && !bLoaderThreadExit)
      {
        oLoadRequestCV.wait(lock);
      }

      if (bLoaderThreadExit)
      {
        break;
      }

      pRequest = qRequestQueue.front();
      qRequestQueue.pop();
    }

    ProcessLoadRequest(pRequest);
  }
}

//-----------------------------------------------------------------------------

static void ProcessLoadRequest(ResourceLoadRequestBase* _pRequest)
{
  ResourceType eType = _pRequest->eType;
  switch (eType)
  {
  case ResourceType::MESH:
  {
    auto* pRequest = static_cast<ResourceLoadRequest<MeshLoadRequestInfo>*>(_pRequest);
    const MeshLoadRequestInfo& oInfo = pRequest->oRequestInfo;

    // TODO: Load from disk. For now the caller hands us the data directly.

    // Request GPU upload for the mesh data. The render thread will call ResourceCompletedCallback when the GPU upload is complete.
    MeshCreateRequest oCreateRequest = {};
    oCreateRequest.hHandle = pRequest->hHandle;
    oCreateRequest.eType = ResourceType::MESH;
    oCreateRequest.pVertexData = oInfo.pVertexData;
    oCreateRequest.zVertexDataSize = oInfo.zVertexDataSize;
    oCreateRequest.pIndexData = oInfo.pIndexData;
    oCreateRequest.zIndexDataSize = oInfo.zIndexDataSize;
    RequestCreateMesh(oCreateRequest);

    delete(pRequest);
    break;
  }
  case ResourceType::MATERIAL:
  {
    auto* pRequest = static_cast<ResourceLoadRequest<MaterialLoadRequestInfo>*>(_pRequest);
    const MaterialLoadRequestInfo& oInfo = pRequest->oRequestInfo;

    // Same here, request GPU to create pipeline, descriptors, etc. The render thread will call ResourceCompletedCallback when the GPU upload is complete.
    MaterialCreateRequest oCreateRequest = {};
    oCreateRequest.hHandle = pRequest->hHandle;
    oCreateRequest.eType = ResourceType::MATERIAL;

    RenderStateInfo& rRSInfo = oCreateRequest.oRenderStateInfo;
    rRSInfo.m_sVSFilename = oInfo.pVSFilename;
    rRSInfo.m_sPSFilename = oInfo.pPSFilename;
    rRSInfo.m_bBlendEnabled = oInfo.bBlendEnabled;
    rRSInfo.m_bDepthWrite = false;
    rRSInfo.m_bDepthRead = false;
    rRSInfo.m_eCullMode = CullMode::NONE;
    rRSInfo.m_eDepthCompareOp = DepthCompareOp::ALWAYS;

    RequestCreateMaterial(oCreateRequest);

    delete(pRequest);
    break;
  }
  default:
    // Handle invalid resource type
    break;
  }
}
