#pragma once

#include <vulkan/vulkan.h>

// VkResult checks, active in every build.
//
//   VK_OK(expr)          Creation and setup: logs a failure and evaluates to false, so
//                        the caller can clean up and return failure:
//                          if (!VK_OK(vkCreateBuffer(...))) return false;
//   VK_CHECK_FATAL(expr) Runtime paths (recording, submitting, presenting, waiting):
//                        a failure there leaves the device in an unknown state.

namespace api
{
  // Logs the failed call with the VkResult's name. Returns _eResult == VK_SUCCESS.
  bool VulkanSucceeded(VkResult _eResult, const char* _sExpression, const char* _sFile, int _iLine);

  [[noreturn]] void VulkanFatal(VkResult _eResult, const char* _sExpression, const char* _sFile, int _iLine);
}

#define VK_OK(expr) ::api::VulkanSucceeded((expr), #expr, __FILE__, __LINE__)

#define VK_CHECK_FATAL(expr)                                       \
  do                                                               \
  {                                                                \
    const VkResult eCheckResult_ = (expr);                         \
    if (eCheckResult_ != VK_SUCCESS)                               \
    {                                                              \
      ::api::VulkanFatal(eCheckResult_, #expr, __FILE__, __LINE__); \
    }                                                              \
  } while (0)
