#include "Mailbox.h"

#include <cstring>

#include "Asserts.h"
#include "Log.h"
#include "Memory/MemoryUtil.h"

// Defined by the owner of the mailboxes (Renderer.cpp)
Mailbox& GetMailbox(MailboxHandle _hMailbox);

//-----------------------------------------------------------------------------

bool AddMessageToMailboxInternal(MailboxHandle _hMailbox, uint32_t _uType, const void* _pMessage, size_t _zSize, size_t _zAlignment)
{
  Mailbox& rMailbox = GetMailbox(_hMailbox);

  const size_t zHeaderSize = sizeof(MailboxMessageHeader);

  void* pHeaderOffset = static_cast<char*>(rMailbox.pData) + rMailbox.zOffset;
  void* pPayloadOffset = AlignUp(static_cast<char*>(pHeaderOffset) + zHeaderSize, _zAlignment);

  const size_t zPadding = static_cast<char*>(pPayloadOffset) - (static_cast<char*>(pHeaderOffset) + zHeaderSize);

  const size_t zRequiredSize = zHeaderSize + zPadding + _zSize;

  if (rMailbox.zOffset > rMailbox.zCapacity ||
    zRequiredSize > rMailbox.zCapacity - rMailbox.zOffset)
  {
    // Dropping silently would desync update and render state (e.g. a handle
    // returned for a render object that never gets created), so make it loud.
    // Logged in every build; the assert adds the dialog in Debug
    LogError("Mailbox %u full: dropped message type %u (%zu bytes, %zu/%zu used)",
      _hMailbox, _uType, zRequiredSize, rMailbox.zOffset, rMailbox.zCapacity);
    ASSERT_MSG(false, "Mailbox full, increase MAILBOX_CAPACITY");
    return false;
  }

  MailboxMessageHeader oHeader;
  oHeader.uType = _uType;
  oHeader.uSize = static_cast<uint32_t>(_zSize);
  oHeader.uAlignmentPadding = static_cast<uint32_t>(zPadding);

  memcpy(pHeaderOffset, &oHeader, zHeaderSize);

  memcpy(pPayloadOffset, _pMessage, _zSize);

  rMailbox.zOffset += zRequiredSize;

  return true;
}

//-----------------------------------------------------------------------------

bool PullMessageFromMailbox(Mailbox* _pMailbox, size_t& _zReadOffset_, const MailboxMessageHeader** ppHeader_, const void** ppData_)
{
  if (_zReadOffset_ >= _pMailbox->zOffset)
  {
    return false;
  }

  MailboxMessageHeader* pHeader = reinterpret_cast<MailboxMessageHeader*>(reinterpret_cast<uintptr_t>(_pMailbox->pData) + _zReadOffset_);
  *ppHeader_ = pHeader;

  size_t zDataOffset = sizeof(MailboxMessageHeader) + pHeader->uAlignmentPadding;

  ASSERT_MSG(_zReadOffset_ + zDataOffset + pHeader->uSize <= _pMailbox->zOffset, "Corrupted mailbox message");

  *ppData_ = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(pHeader) + zDataOffset);

  _zReadOffset_ += zDataOffset + pHeader->uSize;

  return true;
}
