#include "Memory/ThreadArenas.h"

#include <cstdint>

#include "Asserts.h"
#include "Platform/Memory.h"

//-----------------------------------------------------------------------------

static size_t RoundUp(size_t _zValue, size_t _zMultiple)
{
  return (_zValue + (_zMultiple - 1u)) & ~(_zMultiple - 1u);
}

//-----------------------------------------------------------------------------

ThreadArenas* CreateThreadArenas(const ThreadArenasConfig& _oConfig)
{
  // Keep every block start aligned well past anything we allocate
  constexpr size_t BLOCK_ALIGNMENT = 64u;

  const size_t zHeaderSize = RoundUp(sizeof(ThreadArenas), BLOCK_ALIGNMENT);
  const size_t zGlobalSize = RoundUp(_oConfig.zGlobalSize, BLOCK_ALIGNMENT);
  const size_t zSceneSize = RoundUp(_oConfig.zSceneSize, BLOCK_ALIGNMENT);
  const size_t zFrameSize = RoundUp(_oConfig.zFrameSize, BLOCK_ALIGNMENT);

  const size_t zTotalSize = zHeaderSize + zGlobalSize + zSceneSize + zFrameSize;
  char* pBlock = static_cast<char*>(platform::Allocate(zTotalSize));
  FATAL_ASSERT_MSG_FORMAT(pBlock, "The OS refused %zu bytes for thread arenas", zTotalSize);

  char* pGlobal = pBlock + zHeaderSize;
  char* pScene = pGlobal + zGlobalSize;
  char* pFrame = pScene + zSceneSize;

  return new (pBlock) ThreadArenas{
    LinearAllocator(pGlobal, zGlobalSize),
    LinearAllocator(pScene, zSceneSize),
    LinearAllocator(pFrame, zFrameSize)
  };
}

//-----------------------------------------------------------------------------

void DestroyThreadArenas(ThreadArenas* _pArenas)
{
  // LinearAllocator is trivially destructible, nothing to run first
  platform::Free(_pArenas);
}
