#include "API/Vulkan/VulkanMacros.h"

#include <vulkan/vk_enum_string_helper.h>

#include "Asserts.h"
#include "Log.h"

namespace api
{
  bool VulkanSucceeded(VkResult _eResult, const char* _sExpression, const char* _sFile, int _iLine)
  {
    if (_eResult == VK_SUCCESS)
    {
      return true;
    }

    LogError("[Vulkan] %s failed: %s\n  %s(%d)", _sExpression, string_VkResult(_eResult), _sFile, _iLine);
    return false;
  }

  void VulkanFatal(VkResult _eResult, const char* _sExpression, const char* _sFile, int _iLine)
  {
    assert_internal::ReportFatal(_sFile, _iLine, _sExpression, "[Vulkan] %s", string_VkResult(_eResult));
  }
}
