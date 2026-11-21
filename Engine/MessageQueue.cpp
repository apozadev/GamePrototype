#include "MessageQueue.h"

#include "Log.h"
#include "Memory/LinearAllocator.h"
#include "Memory/RingAllocator.h"

// RingAllocator requirement for its memory
static constexpr size_t RING_MEMORY_ALIGNMENT = 16u;

//-----------------------------------------------------------------------------

void InitMessageQueue(MessageQueue* _pQueue, LinearAllocator* _pAllocator, size_t _zSize,
  std::mutex* _pMutex, std::condition_variable* _pDataCV)
{
  char* pRingMemory = static_cast<char*>(_pAllocator->Allocate(_zSize, RING_MEMORY_ALIGNMENT));
  FATAL_ASSERT_MSG_FORMAT(pRingMemory, "Not enough memory for a message queue (%zu bytes)", _zSize);
  RingAllocator* pRing = NewObject<RingAllocator>(_pAllocator, pRingMemory, _zSize);

  std::lock_guard<std::mutex> lock(*_pMutex);
  _pQueue->pRing = pRing;
  _pQueue->pMutex = _pMutex;
  _pQueue->pDataCV = _pDataCV;
}

//-----------------------------------------------------------------------------

void* BeginQueueMessageInternal(MessageQueue* _pQueue, uint32_t _uType, size_t _zPayloadSize, size_t _zPayloadAlignment)
{
  FATAL_ASSERT_MSG(_pQueue->pRing, "InitMessageQueue was not called");

  // The allocation is _zPayloadAlignment-aligned, so the payload is too at a multiple of it
  const size_t zPayloadOffset = AlignUpSize(sizeof(MessageHeader), _zPayloadAlignment);
  const size_t zSize = zPayloadOffset + _zPayloadSize;

  std::unique_lock<std::mutex> lock(*_pQueue->pMutex);

  if (!_pQueue->pRing->Fits(zSize, _zPayloadAlignment))
  {
    LogError("Message type %u of %zu bytes can never fit in its queue: increase the queue size", _uType, zSize);
    return nullptr;
  }

  void* pMemory = nullptr;
  while ((pMemory = _pQueue->pRing->Allocate(zSize, _zPayloadAlignment)) == nullptr)
  {
    _pQueue->oSpaceCV.wait(lock);
  }

  MessageHeader* pHeader = static_cast<MessageHeader*>(pMemory);
  pHeader->uType = _uType;
  pHeader->uPayloadOffset = static_cast<uint32_t>(zPayloadOffset);
  pHeader->zPayloadSize = _zPayloadSize;

  // The consumer can't see it until EndQueueMessage publishes it, so it's filled in unlocked
  return static_cast<char*>(pMemory) + zPayloadOffset;
}

//-----------------------------------------------------------------------------

void EndQueueMessage(MessageQueue* _pQueue)
{
  {
    std::lock_guard<std::mutex> lock(*_pQueue->pMutex);
    _pQueue->pRing->Publish();
  }
  // All: the data CV may be shared with other waiters (e.g. the update thread on the render CV)
  _pQueue->pDataCV->notify_all();
}

//-----------------------------------------------------------------------------

const MessageHeader* PeekQueueMessageLocked(MessageQueue* _pQueue)
{
  return static_cast<const MessageHeader*>(_pQueue->pRing->Peek());
}

//-----------------------------------------------------------------------------

void FreeQueueMessage(MessageQueue* _pQueue, const MessageHeader* _pHeader)
{
  {
    std::lock_guard<std::mutex> lock(*_pQueue->pMutex);
    _pQueue->pRing->Free(const_cast<MessageHeader*>(_pHeader));
  }
  // Wakes the producer if it is waiting for room
  _pQueue->oSpaceCV.notify_one();
}
