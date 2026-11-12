#pragma once

#include <string>

namespace file
{
  class InFile
  {
  public:    
    // GetData() is nullptr if the file couldn't be read
    InFile(const char* _sFilename, bool _bRelativePath = true);
    ~InFile();
    InFile(const InFile&) = delete;
    InFile& operator=(const InFile&) = delete;

    const std::string& GetFilename() const { return m_sFilename; }
    char* GetData() const { return m_pData; }
    size_t GetSize() const { return m_uSize; }

  private:
    std::string m_sFilename;
    char* m_pData;
    size_t m_uSize;
  };
}