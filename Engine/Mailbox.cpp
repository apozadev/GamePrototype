#include "Mailbox.h"

#include <cstring>
#include <assert.h>

// Defined by the owner of the mailboxes (Renderer.cpp)
Mailbox& GetMailbox(MailboxHandle _hMailbox);

//-----------------------------------------------------------------------------

static void* AlignUp(void* _pAddress, size_t _zAlignment)
{
  uintptr_t addr = reinterpret_cast<uintptr_t>(_pAddress);
  uintptr_t alignedAddr = (addr + (_zAlignment - 1)) & ~(_zAlignment - 1);
  return reinterpret_cast<void*>(alignedAddr);
}

//-----------------------------------------------------------------------------

void AddMessageToMailboxInternal(MailboxHandle _hMailbox, uint32_t _uType, const void* _pMessage, size_t _zSize, size_t _zAlignment)
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
    // TODO: Handle insufficient capacity.
    return;
  }

  MailboxMessageHeader oHeader;
  oHeader.uType = _uType;
  oHeader.uSize = static_cast<uint32_t>(_zSize);
  oHeader.uAlignmentPadding = static_cast<uint32_t>(zPadding);

  memcpy(pHeaderOffset, &oHeader, zHeaderSize);

  memcpy(pPayloadOffset, _pMessage, _zSize);

  rMailbox.zOffset += zRequiredSize;
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

  assert(_zReadOffset_ + zDataOffset + pHeader->uSize <= _pMailbox->zOffset);

  *ppData_ = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(pHeader) + zDataOffset);

  _zReadOffset_ += zDataOffset + pHeader->uSize;

  return true;
}
