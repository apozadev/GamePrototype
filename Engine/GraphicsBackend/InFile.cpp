#include <stdlib.h>

#include "InFile.h"
#include "FileUtils.h"

namespace file
{
  InFile::InFile(const char* _sFilename, bool _bRelativePath)
  {
    m_sFilename = std::string(_sFilename);
    m_pData = file::LoadFile(_sFilename, m_uSize, _bRelativePath);
  }

  InFile::~InFile()
  {
    if (m_pData = nullptr)
    {
      free(m_pData);
    }
  }
}