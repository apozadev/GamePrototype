#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <type_traits>

#include "Asserts.h"
#include "Memory/MemoryUtil.h"

class RingAllocator;
struct LinearAllocator;

// Single-producer/single-consumer queue of typed, variable-size messages over a RingAllocator.
// Messages are freed one by one, in order, so the producer can keep writing while the consumer
// works through older ones; a full queue blocks the producer until the consumer frees room.
//
// A message is a trivially copyable struct with a static constexpr TYPE (any enum, cast to
// uint32_t), optionally followed by extra bytes (strings, vertex data...) the struct points into.
// Entry layout inside the ring allocation: [MessageHeader][T][extra bytes].
//
// Every function locks *pMutex itself except PeekQueueMessageLocked. The mutex and data CV
// are external so a consumer can wait on this queue and other conditions with a single CV.

// Extra bytes start at this alignment, enough for any payload (vertex data, strings)
static constexpr size_t MESSAGE_EXTRA_ALIGNMENT = 16u;

struct MessageHeader
{
  uint32_t uType;
  uint32_t uPayloadOffset; // From the header to T
  size_t zPayloadSize;     // T + extra bytes
};

struct MessageQueue
{
  RingAllocator* pRing = nullptr;
  std::mutex* pMutex = nullptr;
  std::condition_variable* pDataCV = nullptr; // Notified (all) on every EndQueueMessage
  std::condition_variable oSpaceCV;           // Producer waits here while the ring is full
};

// Allocates a ring of _zSize bytes from _pAllocator, which must outlive both threads.
// Call before starting the producer and consumer threads.
void InitMessageQueue(MessageQueue* _pQueue, LinearAllocator* _pAllocator, size_t _zSize,
  std::mutex* _pMutex, std::condition_variable* _pDataCV);

// Type-erased, use BeginQueueMessage<T> instead
void* BeginQueueMessageInternal(MessageQueue* _pQueue, uint32_t _uType, size_t _zPayloadSize, size_t _zPayloadAlignment);

// Producer. Reserves a T (value-initialized) plus _zExtraSize bytes at *ppExtra_, blocking until
// there's room. Returns nullptr (logged) if the message can never fit in the ring.
// Fill it in, then call EndQueueMessage. One message in flight at a time.
template<typename T>
T* BeginQueueMessage(MessageQueue* _pQueue, size_t _zExtraSize = 0u, char** ppExtra_ = nullptr)
{
  static_assert(std::is_trivially_copyable_v<T>);
  ASSERT_MSG(_zExtraSize == 0u || ppExtra_, "Extra bytes requested without an out-pointer");

  const size_t zExtraOffset = AlignUpSize(sizeof(T), MESSAGE_EXTRA_ALIGNMENT);
  const size_t zAlignment = alignof(T) > MESSAGE_EXTRA_ALIGNMENT ? alignof(T) : MESSAGE_EXTRA_ALIGNMENT;

  void* pPayload = BeginQueueMessageInternal(_pQueue, static_cast<uint32_t>(T::TYPE), zExtraOffset + _zExtraSize, zAlignment);
  if (!pPayload)
  {
    return nullptr;
  }

  if (ppExtra_)
  {
    *ppExtra_ = static_cast<char*>(pPayload) + zExtraOffset;
  }
  return new (pPayload) T();
}

// Producer. Hands the message started with BeginQueueMessage to the consumer.
void EndQueueMessage(MessageQueue* _pQueue);

// Consumer, with *pMutex held. Oldest message not freed yet, or nullptr.
const MessageHeader* PeekQueueMessageLocked(MessageQueue* _pQueue);

template<typename T>
const T* GetQueueMessagePayload(const MessageHeader* _pHeader)
{
  ASSERT_MSG_FORMAT(_pHeader->uType == static_cast<uint32_t>(T::TYPE), "Message type %u read as type %u",
    _pHeader->uType, static_cast<uint32_t>(T::TYPE));
  return reinterpret_cast<const T*>(reinterpret_cast<const char*>(_pHeader) + _pHeader->uPayloadOffset);
}

// Consumer. Frees the message (and its extra bytes); _pHeader must be what Peek returned.
void FreeQueueMessage(MessageQueue* _pQueue, const MessageHeader* _pHeader);
