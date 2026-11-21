#include "ResourceRegistry.h"

#include <atomic>
#include <mutex>

#include "Asserts.h"

// Each slot packs its generation and state in one word, so readers see both consistently:
//   bits 8..27: generation (never 0 once used, so no handle equals INVALID_RESOURCE_HANDLE)
//   bits 0..7:  ResourceState
static constexpr uint32_t GENERATION_BITS = 32u - RESOURCE_INDEX_BITS;
static constexpr uint32_t GENERATION_MASK = (1u << GENERATION_BITS) - 1u;
static constexpr uint32_t STATE_BITS = 8u;
static constexpr uint32_t STATE_MASK = (1u << STATE_BITS) - 1u;

static std::atomic<uint32_t> s_aSlots[MAX_RESOURCES];

// Guarded by s_oFreeListMutex. Slots below s_uUsedSlotCount have been handed out at least once.
static std::mutex s_oFreeListMutex;
static uint16_t s_aFreeList[MAX_RESOURCES];
static uint32_t s_uFreeCount = 0u;
static uint32_t s_uUsedSlotCount = 0u;

static_assert(MAX_RESOURCES <= UINT16_MAX + 1u, "Free list stores 16-bit indices");

//-----------------------------------------------------------------------------

static uint32_t GetGeneration(ResourceHandle _hHandle)
{
  return _hHandle >> RESOURCE_INDEX_BITS;
}

static uint32_t PackSlot(uint32_t _uGeneration, ResourceState _eState)
{
  return (_uGeneration << STATE_BITS) | static_cast<uint32_t>(_eState);
}

static uint32_t GetSlotGeneration(uint32_t _uSlot)
{
  return _uSlot >> STATE_BITS;
}

//-----------------------------------------------------------------------------

ResourceHandle AllocateResourceHandle()
{
  uint32_t uIndex = 0u;
  {
    std::lock_guard<std::mutex> lock(s_oFreeListMutex);
    if (s_uFreeCount > 0u)
    {
      uIndex = s_aFreeList[--s_uFreeCount];
    }
    else
    {
      FATAL_ASSERT_MSG_FORMAT(s_uUsedSlotCount < MAX_RESOURCES, "Out of resource handles (%u): increase RESOURCE_INDEX_BITS", MAX_RESOURCES);
      uIndex = s_uUsedSlotCount++;
    }
  }

  // The slot is ours: no other thread writes it until it's freed again
  uint32_t uGeneration = GetSlotGeneration(s_aSlots[uIndex].load(std::memory_order_relaxed));
  if (uGeneration == 0u)
  {
    uGeneration = 1u;
  }
  s_aSlots[uIndex].store(PackSlot(uGeneration, ResourceState::LOADING), std::memory_order_release);

  return (uGeneration << RESOURCE_INDEX_BITS) | uIndex;
}

void FreeResourceHandle(ResourceHandle _hHandle)
{
  const uint32_t uIndex = GetResourceIndex(_hHandle);
  const uint32_t uSlot = s_aSlots[uIndex].load(std::memory_order_relaxed);
  if (GetSlotGeneration(uSlot) != GetGeneration(_hHandle))
  {
    ASSERT_MSG_FORMAT(false, "Freeing stale resource handle 0x%08x", _hHandle);
    return;
  }

  uint32_t uNextGeneration = (GetGeneration(_hHandle) + 1u) & GENERATION_MASK;
  if (uNextGeneration == 0u)
  {
    uNextGeneration = 1u;
  }
  s_aSlots[uIndex].store(PackSlot(uNextGeneration, ResourceState::INVALID), std::memory_order_release);

  std::lock_guard<std::mutex> lock(s_oFreeListMutex);
  s_aFreeList[s_uFreeCount++] = static_cast<uint16_t>(uIndex);
}

ResourceState GetResourceState(ResourceHandle _hHandle)
{
  const uint32_t uSlot = s_aSlots[GetResourceIndex(_hHandle)].load(std::memory_order_acquire);
  if (_hHandle == INVALID_RESOURCE_HANDLE || GetSlotGeneration(uSlot) != GetGeneration(_hHandle))
  {
    return ResourceState::INVALID;
  }
  return static_cast<ResourceState>(uSlot & STATE_MASK);
}

//-----------------------------------------------------------------------------

static void SetResourceState(ResourceHandle _hHandle, ResourceState _eState)
{
  std::atomic<uint32_t>& rSlot = s_aSlots[GetResourceIndex(_hHandle)];
  if (GetSlotGeneration(rSlot.load(std::memory_order_relaxed)) != GetGeneration(_hHandle))
  {
    ASSERT_MSG_FORMAT(false, "Setting the state of stale resource handle 0x%08x", _hHandle);
    return;
  }
  rSlot.store(PackSlot(GetGeneration(_hHandle), _eState), std::memory_order_release);
}

void ResourceCompletedCallback(ResourceHandle _hHandle)
{
  SetResourceState(_hHandle, ResourceState::READY);
}

void ResourceFailedCallback(ResourceHandle _hHandle)
{
  SetResourceState(_hHandle, ResourceState::FAILED);
}
