#include "API/DX11/DX11Macros.h"

#include "3rd/dxerr/dxerr.h"
#include "Asserts.h"
#include "Log.h"

namespace api
{
  bool DX11Succeeded(HRESULT _hResult, const char* _sExpression, const char* _sFile, int _iLine)
  {
    if (SUCCEEDED(_hResult))
    {
      return true;
    }

    char aDescription[512];
    DXGetErrorDescriptionA(_hResult, aDescription, sizeof(aDescription));

    LogError("[DX11] %s failed: 0x%08lX %s (%s)\n  %s(%d)",
      _sExpression, static_cast<unsigned long>(_hResult), DXGetErrorStringA(_hResult), aDescription, _sFile, _iLine);
    return false;
  }

  void DX11Fatal(HRESULT _hResult, const char* _sExpression, const char* _sFile, int _iLine)
  {
    char aDescription[512];
    DXGetErrorDescriptionA(_hResult, aDescription, sizeof(aDescription));

    assert_internal::ReportFatal(_sFile, _iLine, _sExpression, "[DX11] 0x%08lX %s\n%s",
      static_cast<unsigned long>(_hResult), DXGetErrorStringA(_hResult), aDescription);
  }
}
