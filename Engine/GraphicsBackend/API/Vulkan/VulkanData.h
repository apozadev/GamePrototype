#pragma once

#include <vulkan/vulkan.h>

#include "API/Vulkan/Window.h"
#include "API/Vulkan/DescriptorUtils.h"
#include "API/Vulkan/RenderTargetBuilder.h"

namespace api
{
  
  struct RenderState;
  struct ResourceSet;
  struct RenderTarget;  

  struct VulkanData
  {
    VkInstance m_hInstance;   

    Window m_oWindow;

    uint32_t m_uRenderQueueFamilyIdx;
    uint32_t m_uPresentQueueFamilyIdx;

    VkPhysicalDevice m_hPhysicalDevice;   

    VkSampleCountFlagBits m_eMaxMsaaSampleCount;

    DescriptorSetUpdater m_oDescSetUpdater;

    RenderTargetBuilder m_oRenderTargetBuilder;

    uint32_t m_uAPIMinorVersion;
    uint32_t m_uAPIMayorVersion;

    bool m_bRenderBegan = false;

  };

}
