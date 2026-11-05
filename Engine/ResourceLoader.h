#pragma once

#include "Vertex.h"
#include <inttypes.h>
#include <cstdlib>
#include <string.h>
#include <unordered_map>
#include <queue>
#include <condition_variable>

#include "Mailbox.h"

typedef uint32_t ResourceHandle;

constexpr ResourceHandle INVALID_RESOURCE_HANDLE = 0;

enum class ResourceType
{
  INVALID,
  MESH,
  MATERIAL,
  TEXTURE,
  COUNT
};

enum class ResourceState
{
  INVALID,
  LOADING,
  READY,
  FAILED,
  COUNT
};

struct MeshLoadRequestInfo
{  
  static constexpr ResourceType eType = ResourceType::MESH;
  const void* pVertexData;
  size_t zVertexDataSize;
  const void* pIndexData;
  size_t zIndexDataSize;
};

struct MaterialLoadRequestInfo
{
  static constexpr ResourceType eType = ResourceType::MATERIAL;
  // TODO: Lifetime. Must stay alive until the material is READY (string literals for now)
  const char* pVSFilename;
  const char* pPSFilename;
  bool bBlendEnabled;
};

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

ResourceHandle LoadResourceAsyncInternal(ResourceLoadRequestBase* _pRequest, ResourceType _eType);

template <typename T>
ResourceHandle LoadResourceAsync(T&& _oRequestInfo)
{
  using Info_t = std::remove_cvref_t<T>;

  static_assert(requires {
    Info_t::eType;
  });

  ResourceLoadRequest<Info_t>* pRequest = new ResourceLoadRequest<Info_t>();
  pRequest->eType = Info_t::eType;
  pRequest->oRequestInfo = std::forward<T>(_oRequestInfo);

  return LoadResourceAsyncInternal(pRequest, Info_t::eType);
}

ResourceState GetResourceState(ResourceHandle _hHandle);

void ResourceCompletedCallback(ResourceHandle _hHandle);

void WorkerThreadProcessLoadRequests();

// Call from the main thread, then join the loader thread.
void RequestLoaderThreadExit();
