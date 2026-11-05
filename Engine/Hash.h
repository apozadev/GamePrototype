#pragma once

#include <cstdint>

// FNV-1a. constexpr so literal names can be hashed at compile time.
constexpr uint32_t HashName(const char* _sName)
{
  uint32_t uHash = 2166136261u;
  for (const char* p = _sName; *p != '\0'; ++p)
  {
    uHash ^= static_cast<uint8_t>(*p);
    uHash *= 16777619u;
  }
  return uHash;
}
