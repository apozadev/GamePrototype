#include "API/Vulkan/VulkanShaderReflection.h"
#include "API/Vulkan/DescriptorUtils.h"
#include "API/Vulkan/VulkanPools.h"

#include "Exception.h"

#include "3rd/spirv-reflect/spirv_reflect.h"

#include <string>

#define CHECK_REFLECT(x, msg) if (!(x)) { THROW_GENERIC_EXCEPTION(msg) }

namespace api
{

void ReflectSetLayouts(const SpvReflectShaderModule& _oReflection, DescriptorSetLayoutBuilder(&aLayoutBuilder_)[MAX_DESCRIPTOR_SET_LAYOUTS])
{  
  const SpvReflectEntryPoint* pEntryPoint = spvReflectGetEntryPoint(&_oReflection, "main");

  CHECK_REFLECT(pEntryPoint != nullptr, "[API] Error: Unable to find shader entry point: ")
  
  uint32_t uDescSetCount = 0;
  SpvReflectResult eResult = spvReflectEnumerateEntryPointDescriptorSets( &_oReflection, "main", &uDescSetCount, NULL);
  CHECK_REFLECT(eResult == SPV_REFLECT_RESULT_SUCCESS, "[API] Error: Failed to enumerate descriptor sets from shader: ")
  CHECK_REFLECT(uDescSetCount <= MAX_DESCRIPTOR_SET_LAYOUTS, "[API] Error: the shader has more than 4 descriptor sets : ")

  SpvReflectDescriptorSet* aDescSets[MAX_DESCRIPTOR_SET_LAYOUTS];// = new SpvReflectDescriptorSet * [uDescSetCount];

  eResult = spvReflectEnumerateEntryPointDescriptorSets(&_oReflection, "main", &uDescSetCount, aDescSets);
  CHECK_REFLECT(eResult == SPV_REFLECT_RESULT_SUCCESS, "[API] Error: Failed to get descriptor sets from shader: ")  

  for (uint32_t i = 0u; i < uDescSetCount; i++)
  {
    SpvReflectDescriptorSet* pDescSet = aDescSets[i];
    
    //if (pDescSet->set == static_cast<uint32_t>(ResourceFrequency::MATERIAL))
    {
      for (uint32_t j = 0u; j < pDescSet->binding_count; j++)
      {
        SpvReflectDescriptorBinding* pDesc = pDescSet->bindings[j];
        VkDescriptorSetLayoutBinding oBinding = {};
        oBinding.binding = pDesc->binding;
        oBinding.descriptorType = static_cast<VkDescriptorType>(pDesc->descriptor_type);
        oBinding.descriptorCount = 1;
        oBinding.stageFlags = static_cast<VkShaderStageFlagBits>(_oReflection.shader_stage);
        oBinding.pImmutableSamplers = NULL;        
        aLayoutBuilder_[pDescSet->set].AddLayoutBinding(pDesc->block.type_description->type_name, std::move(oBinding));
      }
    }
  }
}

}