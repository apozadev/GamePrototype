#pragma once

// Engine assertions. Use these instead of assert() and instead of exceptions.
//
//   ASSERT(cond)                         Debug only. Dialog: Debug / Ignore / Ignore always / Abort.
//   ASSERT_MSG(cond, "msg")              Same, with a message.
//   ASSERT_MSG_FORMAT(cond, "fmt", ...)  Same, printf-style message.
//
//   FATAL_ASSERT(cond)                   Every build. Logs, shows a dialog and terminates.
//   FATAL_ASSERT_MSG(cond, "msg")
//   FATAL_ASSERT_MSG_FORMAT(cond, "fmt", ...)
//   FATAL_ERROR("msg")                   Unconditional fatal error (never returns).
//   FATAL_ERROR_FORMAT("fmt", ...)
//
// Non-fatal asserts are compiled out when NDEBUG is defined: the condition is NOT
// evaluated, so it must never have side effects. Code after a non-fatal assert must
// still handle the failure (return a zero value, skip the work, ...).
//
// Safe to use from any thread; only one dialog is shown at a time.

#include <atomic>

#if defined(_MSC_VER)
#define DEBUG_BREAK() __debugbreak()
#else
#define DEBUG_BREAK() __builtin_trap()
#endif

#ifdef NDEBUG
#define ENGINE_ASSERTS_ENABLED 0
#else
#define ENGINE_ASSERTS_ENABLED 1
#endif

namespace assert_internal
{
  // Returns true if the caller should break into the debugger.
  // _sFormat may be nullptr (no message).
  bool ReportAssert(std::atomic<bool>* _pIgnoreAlways, const char* _sFile, int _iLine, const char* _sCondition, const char* _sFormat, ...);

  [[noreturn]] void ReportFatal(const char* _sFile, int _iLine, const char* _sCondition, const char* _sFormat, ...);

  // Never defined: only used in unevaluated contexts so that compiled-out asserts
  // still "use" their arguments (no unused variable warnings in Release).
  int SwallowArgs(const char* _sFormat, ...);
}

//-----------------------------------------------------------------------------
// Non-fatal
//-----------------------------------------------------------------------------

#if ENGINE_ASSERTS_ENABLED

#define ENGINE_ASSERT_IMPL(_cond, ...)                                                                      \
  do                                                                                                        \
  {                                                                                                         \
    static std::atomic<bool> s_bAssertIgnoreAlways = false;                                                 \
    if (!(_cond) && !s_bAssertIgnoreAlways.load(std::memory_order_relaxed))                                 \
    {                                                                                                       \
      if (assert_internal::ReportAssert(&s_bAssertIgnoreAlways, __FILE__, __LINE__, #_cond, __VA_ARGS__))   \
      {                                                                                                     \
        DEBUG_BREAK();                                                                                      \
      }                                                                                                     \
    }                                                                                                       \
  } while (0)

#else

#define ENGINE_ASSERT_IMPL(_cond, ...)                          \
  do                                                            \
  {                                                             \
    (void)sizeof(!(_cond));                                     \
    (void)sizeof(assert_internal::SwallowArgs(__VA_ARGS__));    \
  } while (0)

#endif

#define ASSERT(_cond)                           ENGINE_ASSERT_IMPL(_cond, nullptr)
#define ASSERT_MSG(_cond, _sMsg)                ENGINE_ASSERT_IMPL(_cond, "%s", _sMsg)
#define ASSERT_MSG_FORMAT(_cond, _sFormat, ...) ENGINE_ASSERT_IMPL(_cond, _sFormat, __VA_ARGS__)

//-----------------------------------------------------------------------------
// Fatal (every build)
//-----------------------------------------------------------------------------

#define ENGINE_FATAL_ASSERT_IMPL(_cond, ...)                                          \
  do                                                                                  \
  {                                                                                   \
    if (!(_cond))                                                                     \
    {                                                                                 \
      assert_internal::ReportFatal(__FILE__, __LINE__, #_cond, __VA_ARGS__);          \
    }                                                                                 \
  } while (0)

#define FATAL_ASSERT(_cond)                           ENGINE_FATAL_ASSERT_IMPL(_cond, nullptr)
#define FATAL_ASSERT_MSG(_cond, _sMsg)                ENGINE_FATAL_ASSERT_IMPL(_cond, "%s", _sMsg)
#define FATAL_ASSERT_MSG_FORMAT(_cond, _sFormat, ...) ENGINE_FATAL_ASSERT_IMPL(_cond, _sFormat, __VA_ARGS__)

#define FATAL_ERROR(_sMsg)                assert_internal::ReportFatal(__FILE__, __LINE__, nullptr, "%s", _sMsg)
#define FATAL_ERROR_FORMAT(_sFormat, ...) assert_internal::ReportFatal(__FILE__, __LINE__, nullptr, _sFormat, __VA_ARGS__)
