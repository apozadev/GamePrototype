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

  // Turns anything that reaches std::terminate (e.g. an exception escaping the graphics
  // API, or std::bad_alloc from a standard container) into a fatal error with a message.
  // The handler is per thread (MSVC keeps one per thread): call it first thing in every thread.
  void InstallFatalErrorHandlersForThread();

  // Ends the process right away: no destructors, no atexit handlers, other threads stop where they are
  [[noreturn]] void TerminateProcessNow(int _iExitCode);
}
