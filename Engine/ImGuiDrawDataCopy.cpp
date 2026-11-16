#include "ImGuiDrawDataCopy.h"

#include <cstring>
#include <new>
#include <type_traits>

#include "imgui.h"

#include "Memory/LinearAllocator.h"

// Points _rDst at an arena copy of _rSrc's elements. _rDst must not own memory.
template<typename T>
static bool CopyImVector(LinearAllocator* _pAllocator, ImVector<T>& _rDst, const ImVector<T>& _rSrc)
{
  static_assert(std::is_trivially_copyable_v<T>);

  _rDst.Size = 0;
  _rDst.Capacity = 0;
  _rDst.Data = nullptr;

  if (_rSrc.Size == 0)
  {
    return true;
  }

  const size_t zBytes = sizeof(T) * static_cast<size_t>(_rSrc.Size);
  T* pData = static_cast<T*>(_pAllocator->Allocate(zBytes, alignof(T)));
  if (!pData)
  {
    return false;
  }

  memcpy(pData, _rSrc.Data, zBytes);
  _rDst.Data = pData;
  _rDst.Size = _rSrc.Size;
  _rDst.Capacity = _rSrc.Size;
  return true;
}

ImDrawData* CopyImDrawData(LinearAllocator* _pAllocator, const ImDrawData* _pSrc)
{
  void* pDataMemory = _pAllocator->Allocate(sizeof(ImDrawData), alignof(ImDrawData));
  if (!pDataMemory)
  {
    return nullptr;
  }

  // Scalars (counts, display rect, scale) carry over; CmdLists is replaced below
  ImDrawData* pDst = new (pDataMemory) ImDrawData(*_pSrc);
  pDst->CmdLists = nullptr;

  if (_pSrc->CmdListsCount <= 0)
  {
    pDst->CmdListsCount = 0;
    return pDst;
  }

  const size_t zListCount = static_cast<size_t>(_pSrc->CmdListsCount);
  ImDrawList** ppLists = static_cast<ImDrawList**>(_pAllocator->Allocate(sizeof(ImDrawList*) * zListCount, alignof(ImDrawList*)));
  if (!ppLists)
  {
    return nullptr;
  }

  for (size_t i = 0u; i < zListCount; i++)
  {
    const ImDrawList* pSrcList = _pSrc->CmdLists[i];

    void* pListMemory = _pAllocator->Allocate(sizeof(ImDrawList), alignof(ImDrawList));
    if (!pListMemory)
    {
      return nullptr;
    }

    // Zeroed, no shared data: renderers only read the three buffers and the flags
    ImDrawList* pDstList = new (pListMemory) ImDrawList(nullptr);
    pDstList->Flags = pSrcList->Flags;

    if (!CopyImVector(_pAllocator, pDstList->CmdBuffer, pSrcList->CmdBuffer) ||
      !CopyImVector(_pAllocator, pDstList->IdxBuffer, pSrcList->IdxBuffer) ||
      !CopyImVector(_pAllocator, pDstList->VtxBuffer, pSrcList->VtxBuffer))
    {
      return nullptr;
    }

    ppLists[i] = pDstList;
  }

  pDst->CmdLists = ppLists;
  return pDst;
}
