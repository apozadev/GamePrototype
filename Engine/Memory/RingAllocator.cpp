#include "Memory/RingAllocator.h"

#include "Asserts.h"
#include "Memory/MemoryUtil.h"

//-----------------------------------------------------------------------------

RingAllocator::RingAllocator(char* _pMemory, size_t _zSize)
  : m_pMemory(_pMemory), m_uSize(static_cast<uint64_t>(_zSize) & ~(ENTRY_ALIGNMENT - 1u))
{
  ASSERT_MSG(reinterpret_cast<uintptr_t>(_pMemory) % ENTRY_ALIGNMENT == 0u, "RingAllocator memory must be 16-byte aligned");
  ASSERT_MSG(m_uSize > 0u, "RingAllocator needs at least 16 bytes");
}

//-----------------------------------------------------------------------------

RingAllocator::EntryHeader* RingAllocator::GetHeader(uint64_t _uPosition) const
{
  return reinterpret_cast<EntryHeader*>(m_pMemory + (_uPosition % m_uSize));
}

//-----------------------------------------------------------------------------

void RingAllocator::ComputeEntryLayout(uint64_t _uOffset, size_t _zSize, size_t _zAlignment, uint64_t& uDataOffset_, uint64_t& uLength_) const
{
  // Align on the real address, the buffer start is only guaranteed 16-byte aligned
  char* pHeader = m_pMemory + _uOffset;
  char* pData = static_cast<char*>(AlignUp(pHeader + sizeof(EntryHeader), _zAlignment));

  uDataOffset_ = static_cast<uint64_t>(pData - pHeader);

  // Every entry starts 16-byte aligned, so a header always fits before the end of the buffer
  uLength_ = (uDataOffset_ + _zSize + (ENTRY_ALIGNMENT - 1u)) & ~(ENTRY_ALIGNMENT - 1u);
}

//-----------------------------------------------------------------------------

void* RingAllocator::Allocate(size_t _zSize, size_t _zAlignment)
{
  const uint64_t uStart = m_uHead;
  uint64_t uOffset = uStart % m_uSize;

  uint64_t uDataOffset = 0u;
  uint64_t uLength = 0u;
  ComputeEntryLayout(uOffset, _zSize, _zAlignment, uDataOffset, uLength);

  // Doesn't fit before the end of the buffer: skip the rest of it, start over at offset 0
  uint64_t uSkipped = 0u;
  if (uOffset + uLength > m_uSize)
  {
    uSkipped = m_uSize - uOffset;
    uOffset = 0u;
    ComputeEntryLayout(uOffset, _zSize, _zAlignment, uDataOffset, uLength);
    if (uLength > m_uSize)
    {
      return nullptr;
    }
  }

  const uint64_t uEnd = uStart + uSkipped + uLength;
  if (uEnd - m_uTail > m_uSize)
  {
    // Would overwrite allocations the consumer hasn't freed yet
    return nullptr;
  }

  if (uSkipped > 0u)
  {
    // uSkipped >= 16 since offsets are multiples of 16, so the marker fits
    EntryHeader* pMarker = GetHeader(uStart);
    pMarker->uEnd = uStart + uSkipped;
    pMarker->uDataOffset = 0u;
  }

  EntryHeader* pHeader = reinterpret_cast<EntryHeader*>(m_pMemory + uOffset);
  pHeader->uEnd = uEnd;
  pHeader->uDataOffset = uDataOffset;

  m_uHead = uEnd;

  return reinterpret_cast<char*>(pHeader) + uDataOffset;
}

//-----------------------------------------------------------------------------

void RingAllocator::Publish()
{
  m_uPublished = m_uHead;
}

//-----------------------------------------------------------------------------

bool RingAllocator::Fits(size_t _zSize, size_t _zAlignment) const
{
  // An empty ring can always place the entry at offset 0
  uint64_t uDataOffset = 0u;
  uint64_t uLength = 0u;
  ComputeEntryLayout(0u, _zSize, _zAlignment, uDataOffset, uLength);
  return uLength <= m_uSize;
}

//-----------------------------------------------------------------------------

void* RingAllocator::Peek()
{
  while (m_uTail < m_uPublished)
  {
    EntryHeader* pHeader = GetHeader(m_uTail);
    if (pHeader->uDataOffset == 0u)
    {
      // Wrap marker, nothing to hand out. Skipping it frees that space.
      m_uTail = pHeader->uEnd;
      continue;
    }
    return reinterpret_cast<char*>(pHeader) + pHeader->uDataOffset;
  }

  return nullptr;
}

//-----------------------------------------------------------------------------

void RingAllocator::Free([[maybe_unused]] void* _pMemory)
{
  // Allocations are freed strictly in order, so the oldest one is the only candidate
  [[maybe_unused]] void* pOldest = Peek();
  ASSERT_MSG(pOldest != nullptr && pOldest == _pMemory, "RingAllocator: freed out of order");

  m_uTail = GetHeader(m_uTail)->uEnd;
}
