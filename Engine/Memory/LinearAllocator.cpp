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
  // A mark ahead of the current position comes from another allocator, or from a scope
  // that ended out of order (an outer rollback already ran): "rolling back" would
  // re-expose memory that is free, or hand out memory that is still in use.
  const size_t zCurrent = PeekCurrent();
  ASSERT_MSG_FORMAT(_zMark <= zCurrent, "Invalid mark for rollback: %zu is ahead of the current position %zu", _zMark, zCurrent);
  if (_zMark <= zCurrent)
  {
    m_pCurrent = m_pStart + _zMark;
  }
}

void LinearAllocator::Reset()
{
  m_pCurrent = m_pStart;
}
