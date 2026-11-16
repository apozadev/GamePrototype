#pragma once

#include "PipelineStage.h"
#include "API/Vulkan/DescriptorUtils.h"
#include "API/Vulkan/RenderState.h"

namespace api
{

  // Returns false if the shader's descriptor sets can't be read (logged)
  bool ReflectSetLayouts(const SpvReflectShaderModule& _oReflection, DescriptorSetLayoutBuilder (&aLayoutBuilder_)[MAX_DESCRIPTOR_SET_LAYOUTS]);

}