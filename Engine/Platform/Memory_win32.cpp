#include "Platform/Memory.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace platform
{
  void* Allocate(size_t _zSize)
  {
    // Reserve and commit in one go: allocators on top are fixed size.
    // Committed pages only take physical memory once they are touched.
    return VirtualAlloc(nullptr, _zSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  }

  void Free(void* _pMemory)
  {
    if (_pMemory)
    {
      // MEM_RELEASE requires size 0 and the base address returned by VirtualAlloc
      VirtualFree(_pMemory, 0, MEM_RELEASE);
    }
  }
}
