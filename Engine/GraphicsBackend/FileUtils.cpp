#include <fstream>
#include <string>
#include <filesystem>
#include <Windows.h>

#include "FileUtils.h"
#include "Log.h"

namespace file
{
  char* LoadFile(const char* _sFilename, size_t& uSize_, bool _bRelativePath)
  {
    //std::string sFullPath(GetWorkingDirectory() + _sFilename);
    std::string sFullPath;

    if (_bRelativePath)
    {
      //sFullPath += GetWorkingDirectory();
    }

    sFullPath += _sFilename;

    std::ifstream oFile(sFullPath.c_str(), std::ios::ate | std::ios::binary);

    uSize_ = 0u;

    if (!oFile.is_open())
    {
      // Without this, tellg() returns -1 and we read garbage into a huge malloc
      LogError("Failed to open file: %s", _sFilename);
      return nullptr;
    }

    uSize_ = (size_t)oFile.tellg();

    char* pBuffer = (char*)malloc(uSize_);

    oFile.seekg(0);
    oFile.read(pBuffer, uSize_);

    oFile.close();

    return pBuffer;
  }

  const std::string& GetWorkingDirectory()
  {
    static std::string sDir;

    if (sDir.empty())
    {
      /*DWORD uBuffSize = GetCurrentDirectory(NULL, (DWORD) 0u);
      sDir.resize((size_t) uBuffSize);

      GetCurrentDirectory(uBuffSize, (LPSTR) sDir.data());
      sDir[uBuffSize-1] = '\\';

      sDir = STR(CURR_WORKING_DIR);
      sDir += '/';*/
    }

    return sDir;
  }
}