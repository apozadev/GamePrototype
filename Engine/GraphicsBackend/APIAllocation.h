#pragma once

// API objects are allocated with NewObject/NewArray (Memory/LinearAllocator.h),
// which treat running out of space as a fatal error.

#include "Memory/LinearAllocator.h"

namespace api
{
  // Runs the destructor of a half-built object if creation throws, releasing what it
  // holds (e.g. ComPtrs). Call Release() once the object is complete.
  template<typename T>
  class CreationGuard
  {
  public:
    explicit CreationGuard(T* _pObject) : m_pObject(_pObject) {}
    CreationGuard(const CreationGuard&) = delete;
    CreationGuard& operator=(const CreationGuard&) = delete;

    ~CreationGuard()
    {
      if (m_pObject)
      {
        m_pObject->~T();
      }
    }

    T* Release()
    {
      T* pObject = m_pObject;
      m_pObject = nullptr;
      return pObject;
    }

  private:
    T* m_pObject;
  };
}
