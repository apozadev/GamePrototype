#pragma once

// Backend code may throw internally (DX11_CHECK, VK_CHECK, THROW_GENERIC_EXCEPTION),
// but exceptions never reach engine code: every public api:: function that can throw
// runs its body through one of these.
//
//   CallOrFail:  creation and setup. Logs and returns a zero value (nullptr, false, 0):
//                the caller fails that resource and moves on.
//   CallOrFatal: everything else (drawing, presenting, destroying). A failure there
//                leaves the device in an unknown state, so it is a fatal error.

#include <exception>
#include <type_traits>

#include "Asserts.h"
#include "Log.h"

namespace api
{
  template<typename F>
  auto CallOrFail(const char* _sFunction, F&& _fnBody) -> decltype(_fnBody())
  {
    using Result_t = decltype(_fnBody());
    static_assert(!std::is_void_v<Result_t>, "CallOrFail needs a result to report failure with");

    try
    {
      return _fnBody();
    }
    catch (const std::exception& e)
    {
      LogError("[API] %s failed: %s", _sFunction, e.what());
    }
    catch (...)
    {
      LogError("[API] %s failed with an unknown exception", _sFunction);
    }

    return Result_t{};
  }

  template<typename F>
  auto CallOrFatal(const char* _sFunction, F&& _fnBody) -> decltype(_fnBody())
  {
    try
    {
      return _fnBody();
    }
    catch (const std::exception& e)
    {
      FATAL_ERROR_FORMAT("[API] %s failed: %s", _sFunction, e.what());
    }
    catch (...)
    {
      FATAL_ERROR_FORMAT("[API] %s failed with an unknown exception", _sFunction);
    }
  }
}
