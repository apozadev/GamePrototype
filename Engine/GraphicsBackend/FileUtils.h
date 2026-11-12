#pragma once

#include <string>

namespace file
{
  // Returns a malloc'd buffer (free it), or nullptr with uSize_ = 0 if the file can't be read
  char* LoadFile(const char* _sFilename, size_t& uSize_, bool _bRelativePath = true);

  const std::string& GetWorkingDirectory();
}