#pragma once

#include <cstddef>

#include "Log.h"
#include "Platform/File.h"

namespace api
{
  struct ShaderCode
  {
    const void* pData = nullptr;
    size_t zSize = 0u;
  };

  // Reads the compiled shader into _pAllocator. Returns false (logged) if it can't be read.
  inline bool LoadShaderCode(LinearAllocator* _pAllocator, const char* _sFilename, ShaderCode& oCode_)
  {
    if (!_sFilename || _sFilename[0] == '\0')
    {
      LogError("Missing shader filename");
      return false;
    }
    oCode_.pData = platform::ReadFile(_sFilename, _pAllocator, oCode_.zSize);
    return oCode_.pData != nullptr;
  }
}
