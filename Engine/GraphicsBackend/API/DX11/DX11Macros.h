#pragma once

#include <d3d11.h>

// HRESULT checks, active in every build.
//
//   DX11_OK(expr)          Creation and setup: logs a failure and evaluates to false, so
//                          the caller can clean up and return failure:
//                            if (!DX11_OK(pDevice->CreateBuffer(...))) return nullptr;
//   DX11_CHECK_FATAL(expr) Runtime paths (drawing, presenting, resizing): a failure there
//                          leaves the device in an unknown state, so it is a fatal error.

namespace api
{
  // Logs the failed call with the HRESULT's name and description. Returns SUCCEEDED(_hResult).
  bool DX11Succeeded(HRESULT _hResult, const char* _sExpression, const char* _sFile, int _iLine);

  [[noreturn]] void DX11Fatal(HRESULT _hResult, const char* _sExpression, const char* _sFile, int _iLine);
}

#define DX11_OK(expr) ::api::DX11Succeeded((expr), #expr, __FILE__, __LINE__)

#define DX11_CHECK_FATAL(expr)                                    \
  do                                                              \
  {                                                               \
    const HRESULT hCheckResult_ = (expr);                         \
    if (FAILED(hCheckResult_))                                    \
    {                                                             \
      ::api::DX11Fatal(hCheckResult_, #expr, __FILE__, __LINE__); \
    }                                                             \
  } while (0)
