#pragma once

#include <cstddef>

struct LinearAllocator;

namespace platform
{
  // Reads the whole file into memory from _pAllocator (max_align_t aligned, not null-terminated).
  // Returns nullptr (logged) if the file can't be read or doesn't fit; _pAllocator is left untouched then.
  void* ReadFile(const char* _sPath, LinearAllocator* _pAllocator, size_t& zSize_);

  // Creates or overwrites the file. Returns false (logged) on failure.
  bool WriteFile(const char* _sPath, const void* _pData, size_t _zSize);
}
