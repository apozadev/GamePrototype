#pragma once

#include <vulkan/vulkan.h>
#include "API/Vulkan/DescriptorUtils.h"

namespace api
{

  struct ResourceLayout
  {
    VkDescriptorSetLayout m_hDescSetLayout;
    DescriptorSetLayoutBuilder m_oLayoutBuilder;
  };

}
