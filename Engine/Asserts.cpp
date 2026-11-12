#include "Asserts.h"

#include <cstdarg>
#include <cstdio>
#include <mutex>

#include "Log.h"
#include "Platform/Debug.h"

namespace
{
  constexpr int FATAL_EXIT_CODE = 3;

  // One dialog at a time, whatever thread asserts
  std::mutex s_oDialogMutex;

  // "<what>: <condition>\n<message>\n\n<file>(<line>)"
  void FormatReport(char* _pBuffer, size_t _zSize, const char* _sWhat, const char* _sFile, int _iLine, const char* _sCondition, const char* _sFormat, va_list _args)
  {
    int iWritten = _sCondition
      ? snprintf(_pBuffer, _zSize, "%s: %s\n", _sWhat, _sCondition)
      : snprintf(_pBuffer, _zSize, "%s\n", _sWhat);
    size_t zOffset = (iWritten > 0) ? static_cast<size_t>(iWritten) : 0u;

    if (_sFormat && zOffset < _zSize)
    {
      iWritten = vsnprintf(_pBuffer + zOffset, _zSize - zOffset, _sFormat, _args);
      zOffset += (iWritten > 0) ? static_cast<size_t>(iWritten) : 0u;
    }

    if (zOffset < _zSize)
    {
      snprintf(_pBuffer + zOffset, _zSize - zOffset, "\n\n%s(%d)", _sFile, _iLine);
    }
  }
}

//-----------------------------------------------------------------------------

namespace assert_internal
{
  bool ReportAssert(std::atomic<bool>* _pIgnoreAlways, const char* _sFile, int _iLine, const char* _sCondition, const char* _sFormat, ...)
  {
    char aMessage[2048];
    va_list args;
    va_start(args, _sFormat);
    FormatReport(aMessage, sizeof(aMessage), "Assertion failed", _sFile, _iLine, _sCondition, _sFormat, args);
    va_end(args);

    LogError("%s", aMessage);

    std::lock_guard<std::mutex> lock(s_oDialogMutex);

    // Another thread may have silenced this assert while we waited for the dialog
    if (_pIgnoreAlways->load(std::memory_order_relaxed))
    {
      return false;
    }

    switch (platform::ShowAssertDialog(aMessage))
    {
    case platform::AssertAction::DEBUG_BREAK:
      return true;
    case platform::AssertAction::IGNORE_ALWAYS:
      _pIgnoreAlways->store(true, std::memory_order_relaxed);
      return false;
    case platform::AssertAction::ABORT:
      platform::TerminateProcessNow(FATAL_EXIT_CODE);
    case platform::AssertAction::IGNORE_ONCE:
    default:
      return false;
    }
  }

  //-----------------------------------------------------------------------------

  void ReportFatal(const char* _sFile, int _iLine, const char* _sCondition, const char* _sFormat, ...)
  {
    char aMessage[2048];
    va_list args;
    va_start(args, _sFormat);
    FormatReport(aMessage, sizeof(aMessage), "Fatal error", _sFile, _iLine, _sCondition, _sFormat, args);
    va_end(args);

    LogError("%s", aMessage);

    {
      std::lock_guard<std::mutex> lock(s_oDialogMutex);
      platform::ShowFatalErrorDialog(aMessage);
    }

    if (platform::IsDebuggerAttached())
    {
      DEBUG_BREAK();
    }

    platform::TerminateProcessNow(FATAL_EXIT_CODE);
  }
}
