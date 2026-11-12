#pragma once

#include <cstddef>

// Platform-side memory: big blocks straight from the OS, meant to back
// allocators (see Memory/LinearAllocator.h, Memory/RingAllocator.h).
// Not for small or frequent allocations.

namespace platform
{
  // Returns zeroed, committed memory aligned to at least the OS page size,
  // or nullptr if the OS refuses.
  void* Allocate(size_t _zSize);

  // Releases a block returned by Allocate. Accepts nullptr.
  void Free(void* _pMemory);
}
