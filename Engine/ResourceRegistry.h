#pragma once

#include <cstdint>

// A handle is a slot index plus the slot's generation. The generation changes every time
// the slot is freed, so a handle to an unloaded resource never matches the slot's next
// occupant: lookups treat it as INVALID instead of returning the new resource.
typedef uint32_t ResourceHandle;

constexpr ResourceHandle INVALID_RESOURCE_HANDLE = 0;

constexpr uint32_t RESOURCE_INDEX_BITS = 12u;
constexpr uint32_t MAX_RESOURCES = 1u << RESOURCE_INDEX_BITS;

// Always < MAX_RESOURCES: tables indexed by it need no bounds check, but must still
// compare the stored handle to reject stale ones.
inline uint32_t GetResourceIndex(ResourceHandle _hHandle)
{
  return _hHandle & (MAX_RESOURCES - 1u);
}

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

// Any thread. Returns a LOADING handle. Running out of slots is a fatal error (increase RESOURCE_INDEX_BITS).
ResourceHandle AllocateResourceHandle();

// Render thread only, once the resource is destroyed. Its handles become INVALID.
void FreeResourceHandle(ResourceHandle _hHandle);

// Any thread, lock-free. INVALID for stale or unknown handles.
ResourceState GetResourceState(ResourceHandle _hHandle);

// Called by the thread that finishes the resource (render thread for GPU resources)
void ResourceCompletedCallback(ResourceHandle _hHandle);

// Moves the resource to FAILED. It never becomes READY; the game should stop waiting on it.
void ResourceFailedCallback(ResourceHandle _hHandle);
