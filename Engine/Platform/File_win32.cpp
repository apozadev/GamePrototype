#include "Platform/File.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "Log.h"
#include "Memory/LinearAllocator.h"

namespace platform
{
  void* ReadFile(const char* _sPath, LinearAllocator* _pAllocator, size_t& zSize_)
  {
    zSize_ = 0u;

    HANDLE hFile = CreateFileA(_sPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
      LogError("Failed to open file %s (error %lu)", _sPath, GetLastError());
      return nullptr;
    }

    void* pResult = nullptr;

    LARGE_INTEGER oFileSize = {};
    if (!GetFileSizeEx(hFile, &oFileSize))
    {
      LogError("Failed to get the size of file %s (error %lu)", _sPath, GetLastError());
    }
    else if (static_cast<unsigned long long>(oFileSize.QuadPart) > MAXDWORD)
    {
      LogError("File %s is too big (%lld bytes)", _sPath, oFileSize.QuadPart);
    }
    else
    {
      const DWORD uSize = static_cast<DWORD>(oFileSize.QuadPart);

      ScopedRollback oRollback(_pAllocator);

      // At least 1 byte so an empty file still gets a valid pointer
      void* pData = _pAllocator->Allocate(uSize > 0u ? uSize : 1u);
      DWORD uBytesRead = 0u;

      if (!pData)
      {
        LogError("Not enough memory to read file %s (%lu bytes)", _sPath, uSize);
      }
      else if (!::ReadFile(hFile, pData, uSize, &uBytesRead, nullptr) || uBytesRead != uSize)
      {
        LogError("Failed to read file %s (error %lu)", _sPath, GetLastError());
      }
      else
      {
        oRollback.Commit();
        zSize_ = uSize;
        pResult = pData;
      }
    }

    CloseHandle(hFile);
    return pResult;
  }

  bool WriteFile(const char* _sPath, const void* _pData, size_t _zSize)
  {
    if (_zSize > MAXDWORD)
    {
      LogError("Data for file %s is too big (%zu bytes)", _sPath, _zSize);
      return false;
    }

    HANDLE hFile = CreateFileA(_sPath, GENERIC_WRITE, 0u, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
      LogError("Failed to create file %s (error %lu)", _sPath, GetLastError());
      return false;
    }

    const DWORD uSize = static_cast<DWORD>(_zSize);
    DWORD uBytesWritten = 0u;
    const bool bSucceeded = ::WriteFile(hFile, _pData, uSize, &uBytesWritten, nullptr) && uBytesWritten == uSize;
    if (!bSucceeded)
    {
      LogError("Failed to write file %s (error %lu)", _sPath, GetLastError());
    }

    CloseHandle(hFile);
    return bSucceeded;
  }
}
