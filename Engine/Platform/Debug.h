#pragma once

// Platform-side support for Assert.h. Engine code uses the ASSERT macros, not these.

namespace platform
{
  enum class AssertAction
  {
    DEBUG_BREAK,
    IGNORE_ONCE,
    IGNORE_ALWAYS,
    ABORT
  };

  // Blocks until the user picks an action
  AssertAction ShowAssertDialog(const char* _sMessage);

  // Blocks until dismissed
  void ShowFatalErrorDialog(const char* _sMessage);

  bool IsDebuggerAttached();

  // Turns standard library failures (the build has no exceptions, so the STL reports
  // errors such as bad_alloc through a handler) and abort()/std::terminate into a fatal
  // error with a message. Process-wide: call once at startup, before starting threads.
  void InstallFatalErrorHandlers();

  // Ends the process right away: no destructors, no atexit handlers, other threads stop where they are
  [[noreturn]] void TerminateProcessNow(int _iExitCode);
}
