#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

struct MailboxMessageHeader
{
  uint32_t uType;
  uint32_t uSize;
  uint32_t uAlignmentPadding;
};

enum MailboxState
{
  MAILBOX_IDLE,
  MAILBOX_RECORDING,
  MAILBOX_READY,
  MAILBOX_PROCESSING
};

struct Mailbox
{
  void* pData;
  size_t zOffset;
  size_t zCapacity;
  MailboxState eState;
};

typedef uint32_t MailboxHandle;

// Type-erased writer, use AddMessageToMailbox<T> instead
void AddMessageToMailboxInternal(MailboxHandle _hMailbox, uint32_t _uType, const void* _pMessage, size_t _zSize, size_t _zAlignment);

template<typename T>
void AddMessageToMailbox(MailboxHandle _hMailbox, const T* _pMessage)
{
  static_assert(std::is_trivially_copyable_v<T>);
  AddMessageToMailboxInternal(_hMailbox, static_cast<uint32_t>(T::TYPE), _pMessage, sizeof(T), alignof(T));
}

bool PullMessageFromMailbox(Mailbox* _pMailbox, size_t& _zReadOffset_, const MailboxMessageHeader** ppHeader_, const void** ppData_);
