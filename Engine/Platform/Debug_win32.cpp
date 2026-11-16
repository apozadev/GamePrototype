#include "Platform/Debug.h"

#include <csignal>
#include <cstdlib>
#include <exception>

#include "Asserts.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

// TaskDialogIndirect (custom buttons) only exists in Common Controls 6.
// It's looked up at runtime, with a MessageBox fallback if it isn't there.
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace platform
{
  //-----------------------------------------------------------------------------

  static constexpr int BUTTON_DEBUG = 100;
  static constexpr int BUTTON_IGNORE = 101;
  static constexpr int BUTTON_IGNORE_ALWAYS = 102;
  static constexpr int BUTTON_ABORT = 103;

  typedef HRESULT(WINAPI* TaskDialogIndirectFn)(const TASKDIALOGCONFIG*, int*, int*, BOOL*);

  static TaskDialogIndirectFn GetTaskDialogIndirect()
  {
    HMODULE hComCtl = LoadLibraryW(L"comctl32.dll");
    if (!hComCtl)
    {
      return nullptr;
    }
    return reinterpret_cast<TaskDialogIndirectFn>(reinterpret_cast<void*>(GetProcAddress(hComCtl, "TaskDialogIndirect")));
  }

  //-----------------------------------------------------------------------------

  // Fallback without TaskDialog: Abort / Retry (= debug) / Ignore, then ask about "always"
  static AssertAction ShowAssertMessageBox(const char* _sMessage)
  {
    const int iResult = MessageBoxA(nullptr, _sMessage, "Assertion failed",
      MB_ABORTRETRYIGNORE | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND);

    switch (iResult)
    {
    case IDABORT:
      return AssertAction::ABORT;
    case IDRETRY:
      return AssertAction::DEBUG_BREAK;
    default:
      break;
    }

    const int iAlways = MessageBoxA(nullptr, "Ignore this assert for the rest of the run?", "Assertion failed",
      MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND);
    return iAlways == IDYES ? AssertAction::IGNORE_ALWAYS : AssertAction::IGNORE_ONCE;
  }

  //-----------------------------------------------------------------------------

  AssertAction ShowAssertDialog(const char* _sMessage)
  {
    TaskDialogIndirectFn pfnTaskDialogIndirect = GetTaskDialogIndirect();
    if (!pfnTaskDialogIndirect)
    {
      return ShowAssertMessageBox(_sMessage);
    }

    // TaskDialog only takes UTF-16
    wchar_t aMessage[4096];
    if (MultiByteToWideChar(CP_UTF8, 0, _sMessage, -1, aMessage, static_cast<int>(sizeof(aMessage) / sizeof(aMessage[0]))) == 0)
    {
      aMessage[0] = L'\0';
    }

    const TASKDIALOG_BUTTON aButtons[] = {
      { BUTTON_DEBUG,         L"Debug" },
      { BUTTON_IGNORE,        L"Ignore" },
      { BUTTON_IGNORE_ALWAYS, L"Ignore always" },
      { BUTTON_ABORT,         L"Abort" },
    };

    TASKDIALOGCONFIG oConfig = {};
    oConfig.cbSize = sizeof(oConfig);
    oConfig.dwFlags = TDF_SIZE_TO_CONTENT;
    oConfig.pszWindowTitle = L"Assertion failed";
    oConfig.pszMainIcon = TD_WARNING_ICON;
    oConfig.pszMainInstruction = L"Assertion failed";
    oConfig.pszContent = aMessage;
    oConfig.pButtons = aButtons;
    oConfig.cButtons = static_cast<UINT>(sizeof(aButtons) / sizeof(aButtons[0]));
    oConfig.nDefaultButton = BUTTON_DEBUG;

    int iButton = 0;
    if (FAILED(pfnTaskDialogIndirect(&oConfig, &iButton, nullptr, nullptr)))
    {
      return ShowAssertMessageBox(_sMessage);
    }

    switch (iButton)
    {
    case BUTTON_DEBUG:         return AssertAction::DEBUG_BREAK;
    case BUTTON_IGNORE_ALWAYS: return AssertAction::IGNORE_ALWAYS;
    case BUTTON_ABORT:         return AssertAction::ABORT;
    default:                   return AssertAction::IGNORE_ONCE; // Includes closing the dialog
    }
  }

  //-----------------------------------------------------------------------------

  void ShowFatalErrorDialog(const char* _sMessage)
  {
    MessageBoxA(nullptr, _sMessage, "Fatal error", MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
  }

  //-----------------------------------------------------------------------------

  bool IsDebuggerAttached()
  {
    return IsDebuggerPresent() != FALSE;
  }

  //-----------------------------------------------------------------------------

  void TerminateProcessNow(int _iExitCode)
  {
    TerminateProcess(GetCurrentProcess(), static_cast<UINT>(_iExitCode));
    // TerminateProcess on the current process doesn't return, but the compiler can't know
    for (;;)
    {
    }
  }

  //-----------------------------------------------------------------------------

  // With _HAS_EXCEPTIONS=0 the STL reports its failures (e.g. bad_alloc, out of range)
  // through this handler instead of throwing; it carries the error message.
  static void __cdecl OnStandardLibraryError(const std::exception& _rError)
  {
    FATAL_ERROR_FORMAT("Standard library error: %s", _rError.what());
  }

  // abort(), std::terminate (e.g. destroying a joinable std::thread), failed CRT checks
  static void __cdecl OnAbortSignal(int)
  {
    FATAL_ERROR("abort() was called");
  }

  void InstallFatalErrorHandlers()
  {
    std::exception::_Set_raise_handler(OnStandardLibraryError);

    // The fatal dialog replaces the CRT's own abort message box
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    signal(SIGABRT, OnAbortSignal);
  }
}
