#pragma once

#include <vulkan/vulkan.h>

#include "3rd/spirv-reflect/spirv_reflect.h"

#include "API/Vulkan/ResourceSet.h"
#include "API/Vulkan/DescriptorUtils.h"

#define MAX_DESCRIPTOR_SET_LAYOUTS 4

namespace api
{
struct RenderState
{
public:
  VkPipelineLayout m_hPipelineLayout;  
  VkPipeline m_hGraphicsPipeline;    

  DescriptorSetLayoutBuilder m_aLayoutBuilders[MAX_DESCRIPTOR_SET_LAYOUTS];
  VkDescriptorSetLayout m_aDescriptorSetLayouts[MAX_DESCRIPTOR_SET_LAYOUTS];

  SpvReflectShaderModule m_oVertexReflection;
  SpvReflectShaderModule m_oGeomReflection;
  SpvReflectShaderModule m_oPixelReflection;
};

}
