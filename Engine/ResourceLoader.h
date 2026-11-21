#pragma once

#include "ResourceRegistry.h"

struct LinearAllocator;

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

// Tracks the loading of a set of resources. It doesn't own them: resources live until UnloadScene.
// Plain data in the caller's allocator, update thread only.
struct LoadGroup;

enum class LoadGroupState
{
  LOADING,
  READY,  // Every resource is READY
  FAILED, // Every resource finished, at least one FAILED (it draws with the error mesh/material)
};

struct LoadGroupStatus
{
  LoadGroupState eState;
  uint32_t uReady;
  uint32_t uFailed; // Includes resources no longer valid (unloaded with the scene)
  uint32_t uTotal;
};

// Allocates the load queue from _pAllocator, which must outlive the loader thread.
// Call once from the main thread before starting the loader thread.
void InitResourceLoader(LinearAllocator* _pAllocator);

// Room for _uCapacity resources. Lives as long as _pAllocator's memory (usually the scene arena).
LoadGroup* CreateLoadGroup(LinearAllocator* _pAllocator, uint32_t _uCapacity);

// Update thread only (the load queue has a single producer).
// Blocks while the load queue is full. A request too big to ever fit in the queue, or a
// full group, is a fatal error (increase LOAD_QUEUE_SIZE or the group capacity).
ResourceHandle LoadResourceAsync(LoadGroup* _pGroup, const MeshLoadRequestInfo& _oInfo);
ResourceHandle LoadResourceAsync(LoadGroup* _pGroup, const MaterialLoadRequestInfo& _oInfo);

// Update thread only. Lock-free: one atomic read per resource in the group.
LoadGroupStatus GetLoadGroupStatus(const LoadGroup* _pGroup);

// Update thread only. Unloads every resource requested before this call; those requested after
// belong to the next scene. Their handles become INVALID once the render thread gets to it, and
// render objects using them stop drawing. Load groups pointing at them must be dropped too.
void UnloadScene();

void WorkerThreadProcessLoadRequests();

// Call from the main thread, then join the loader thread.
void RequestLoaderThreadExit();
