#pragma once

#include <vulkan/vulkan.h>

namespace api
{
  struct RenderState;

  struct ResourceSet
  {
  public:      
      
    VkDescriptorSet* m_pDescSets;

    RenderState* m_pRenderState;

    uint32_t m_uSetId;
  };

}
