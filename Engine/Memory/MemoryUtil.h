#pragma once

#include <cstddef>
#include <cstdint>

inline void* AlignUp(void* _pAddress, size_t _zAlignment)
{
  uintptr_t addr = reinterpret_cast<uintptr_t>(_pAddress);
  uintptr_t alignedAddr = (addr + (_zAlignment - 1)) & ~(_zAlignment - 1);
  return reinterpret_cast<void*>(alignedAddr);
}

inline size_t AlignUpSize(size_t _zValue, size_t _zAlignment)
{
  return (_zValue + (_zAlignment - 1)) & ~(_zAlignment - 1);
}

// Packs several pieces into one allocation: Add returns where each piece
// starts, zSize ends up as the total to allocate.
struct PackedLayout
{
  size_t zSize = 0u;

  size_t Add(size_t _zBytes, size_t _zAlignment)
  {
    zSize = AlignUpSize(zSize, _zAlignment);
    const size_t zOffset = zSize;
    zSize += _zBytes;
    return zOffset;
  }
};
