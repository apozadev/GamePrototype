#pragma once

#include "InFile.h"
#include "PipelineStage.h"
#include "API/Vulkan/DescriptorUtils.h"
#include "API/Vulkan/RenderState.h"

namespace api
{

  void ReflectSetLayouts(const SpvReflectShaderModule& _oReflection, DescriptorSetLayoutBuilder (&aLayoutBuilder_)[MAX_DESCRIPTOR_SET_LAYOUTS]);

}