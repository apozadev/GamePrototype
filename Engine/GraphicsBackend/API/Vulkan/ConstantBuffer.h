#pragma once

#include <vulkan/vulkan.h>

namespace api
{
  struct ConstantBuffer
  {
  public:
    VkBuffer* m_pUniformBuffers;
    VkDeviceMemory* m_pUniformBuffersMemory;
    void** m_pUniformBuffersMapped;    
  };
}