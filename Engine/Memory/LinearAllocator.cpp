#include "Memory/LinearAllocator.h"
#include "Memory/MemoryUtil.h"

#include "Asserts.h"

LinearAllocator::LinearAllocator(char* _pMemory, size_t _zSize)
  : m_pStart(_pMemory), m_pCurrent(_pMemory), m_zSize(_zSize)
{
}

void* LinearAllocator::Allocate(size_t _zSize, size_t _zAlignment)
{
  char* pAlignedCurrent = static_cast<char*>(AlignUp(m_pCurrent, _zAlignment));
  if (pAlignedCurrent + _zSize > m_pStart + m_zSize)
  {
    // Not enough memory left in the allocator
    return nullptr;
  }
  void* pAllocatedMemory = pAlignedCurrent;
  m_pCurrent = pAlignedCurrent + _zSize;
  return pAllocatedMemory;
}

size_t LinearAllocator::PeekCurrent()
{
  return static_cast<size_t>(m_pCurrent - m_pStart);
}

void LinearAllocator::Rollback(size_t _zMark)
{
  ASSERT_MSG_FORMAT(_zMark <= m_zSize, "Invalid mark for rollback: %zu (size %zu)", _zMark, m_zSize);
  if (_zMark <= m_zSize)
  {
    m_pCurrent = m_pStart + _zMark;
  }
}

void LinearAllocator::Reset()
{
  m_pCurrent = m_pStart;
}
