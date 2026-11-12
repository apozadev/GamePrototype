#pragma once

#include <cstddef>
#include <cstdint>

// FIFO allocator over a fixed buffer: allocations are freed in the same order
// they were made. One producer allocates and publishes; one consumer peeks the
// oldest published allocation and frees it. Doubles as a queue of
// variable-size messages, so no separate container is needed.
//
// Not synchronized: producer and consumer must serialize their calls (one mutex
// shared by both is enough). The payload written between Allocate and Publish
// needs no lock, since the consumer can't see an allocation until it is
// published and the producer never reuses memory the consumer hasn't freed.
class RingAllocator
{
public:
  // _pMemory must be 16-byte aligned. The usable size is rounded down to a multiple of 16.
  RingAllocator(char* _pMemory, size_t _zSize);
  RingAllocator(const RingAllocator&) = delete;
  RingAllocator& operator=(const RingAllocator&) = delete;

  // Producer. Returns nullptr if there is no room right now (retry after the consumer frees).
  void* Allocate(size_t _zSize, size_t _zAlignment = alignof(std::max_align_t));

  // Producer. Makes every allocation made so far visible to the consumer.
  void Publish();

  // Whether an allocation could ever succeed, i.e. with the ring empty.
  // A producer that waits for room must check this first or it may wait forever.
  bool Fits(size_t _zSize, size_t _zAlignment = alignof(std::max_align_t)) const;

  // Consumer. Oldest published allocation not freed yet, or nullptr.
  void* Peek();

  // Consumer. Frees the oldest allocation; _pMemory must be what Peek returned.
  void Free(void* _pMemory);

private:

  // Precedes every allocation. A header with uDataOffset == 0 is a wrap marker:
  // the allocation after it didn't fit before the end of the buffer.
  struct EntryHeader
  {
    uint64_t uEnd;        // Position right after this entry
    uint64_t uDataOffset; // From the header to the allocation
  };

  static constexpr uint64_t ENTRY_ALIGNMENT = 16u;

  // Bytes from the header to the data, and total entry length, for an entry at _uOffset
  void ComputeEntryLayout(uint64_t _uOffset, size_t _zSize, size_t _zAlignment, uint64_t& uDataOffset_, uint64_t& uLength_) const;

  EntryHeader* GetHeader(uint64_t _uPosition) const;

  char* m_pMemory;
  uint64_t m_uSize;

  // Positions only grow; the buffer offset is position % m_uSize
  uint64_t m_uHead = 0u;      // Producer: where the next allocation goes
  uint64_t m_uPublished = 0u; // End of the newest allocation the consumer may see
  uint64_t m_uTail = 0u;      // Consumer: start of the oldest allocation
};
