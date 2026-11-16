#pragma once

#include "Vertex.h"
#include <cstddef>
#include <inttypes.h>

struct LinearAllocator;

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

// Filenames are copied into the request, they don't need to outlive the call.
struct MeshLoadRequestInfo
{
  static constexpr ResourceType eType = ResourceType::MESH;
  const char* pFilename; // .obj
};

struct MaterialLoadRequestInfo
{
  static constexpr ResourceType eType = ResourceType::MATERIAL;
  const char* pVSFilename;
  const char* pPSFilename;
  bool bBlendEnabled;
};

// Allocates the load request ring from _pAllocator, which must outlive the loader thread.
// Call once from the main thread before starting the loader thread.
void InitResourceLoader(LinearAllocator* _pAllocator);

// Update thread only (the request ring has a single producer).
// Blocks while the request ring is full. A request too big to ever fit in the ring
// is a fatal error (increase LOAD_REQUEST_RING_SIZE).
ResourceHandle LoadResourceAsync(const MeshLoadRequestInfo& _oInfo);
ResourceHandle LoadResourceAsync(const MaterialLoadRequestInfo& _oInfo);

ResourceState GetResourceState(ResourceHandle _hHandle);

// Called by the thread that finishes the resource (render thread for GPU resources)
void ResourceCompletedCallback(ResourceHandle _hHandle);

// Moves the resource to FAILED. It never becomes READY; the game should stop waiting on it.
void ResourceFailedCallback(ResourceHandle _hHandle);

void WorkerThreadProcessLoadRequests();

// Call from the main thread, then join the loader thread.
void RequestLoaderThreadExit();
