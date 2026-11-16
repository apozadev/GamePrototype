#pragma once

#include <cstddef>
#include <new>
#include <utility>

#include "Asserts.h"

struct LinearAllocator
{
  LinearAllocator(char* memory, size_t size);
  LinearAllocator(const LinearAllocator&) = delete;
  LinearAllocator& operator=(const LinearAllocator&) = delete;

  void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));
  void Reset();
  size_t PeekCurrent();

private:
  friend class ScopedRollback;  
  void Rollback(size_t _zMark);

private:
  char* m_pStart;
  char* m_pCurrent;
  size_t m_zSize;
};

// Typed helpers: allocate and construct in place. Running out of space is a fatal
// error (arena sizes are a budget to fix, not a condition to handle); use
// Allocate directly to probe for space instead.
// Reset/Rollback never run destructors, so an object with a non-trivial destructor
// (e.g. holding ComPtrs) must be destroyed explicitly (_pObj->~T()) before its memory is reused.

template<typename T, typename... Args>
T* NewObject(LinearAllocator* _pAllocator, Args&&... _args)
{
  void* pMemory = _pAllocator->Allocate(sizeof(T), alignof(T));
  FATAL_ASSERT_MSG_FORMAT(pMemory, "Linear allocator out of memory: %zu bytes requested, %zu already used",
    sizeof(T), _pAllocator->PeekCurrent());
  return new (pMemory) T(std::forward<Args>(_args)...);
}

// Value-initialized (zeroed for trivial types)
template<typename T>
T* NewArray(LinearAllocator* _pAllocator, size_t _zCount)
{
  void* pMemory = _pAllocator->Allocate(sizeof(T) * _zCount, alignof(T));
  FATAL_ASSERT_MSG_FORMAT(pMemory, "Linear allocator out of memory: %zu bytes requested, %zu already used",
    sizeof(T) * _zCount, _pAllocator->PeekCurrent());

  T* pArray = static_cast<T*>(pMemory);
  for (size_t i = 0u; i < _zCount; i++)
  {
    new (&pArray[i]) T();
  }
  return pArray;
}

// Hands back everything allocated from _pAllocator during its lifetime, unless Commit()
// is called. Prefer it to PeekCurrent/Rollback: every early return is covered.
//
//   Scratch memory, always given back:      Keep on success, give back on failure:
//     {                                       ScopedRollback oRollback(_pAllocator);
//       ScopedRollback oTemp(_pAllocator);    if (!Build(...)) { Destroy(...); return nullptr; }
//       ...                                   oRollback.Commit();
//     }                                       return pObject;
//
// Like Rollback, it never runs destructors: release what the memory holds (API objects,
// _pObj->~T()) before the scope ends. Declaring the guard first makes it run last.
// Nested guards must end in reverse order of creation, which scopes guarantee.
class ScopedRollback
{
public:
  // [[nodiscard]]: an unnamed ScopedRollback{_pAllocator}; would roll back at once
  [[nodiscard]] explicit ScopedRollback(LinearAllocator* _pAllocator)
    : m_pAllocator(_pAllocator), m_zMark(_pAllocator->PeekCurrent())
  {
  }

  ~ScopedRollback()
  {
    if (m_pAllocator)
    {
      m_pAllocator->Rollback(m_zMark);
    }
  }

  // Keeps everything allocated since construction. The scope does nothing after this.
  void Commit()
  {
    m_pAllocator = nullptr;
  }

  ScopedRollback(const ScopedRollback&) = delete;
  ScopedRollback& operator=(const ScopedRollback&) = delete;

private:
  LinearAllocator* m_pAllocator;
  size_t m_zMark;
};