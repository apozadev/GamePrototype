#pragma once

#include "TypedPool.h"
#include "TypedContiguousPool.h"

#include "API/Vulkan/RenderState.h"
#include "API/Vulkan/ResourceSet.h"
#include "API/Vulkan/ConstantBuffer.h"
#include "API/Vulkan/Mesh.h"
#include "API/Vulkan/Texture.h"
#include "API/Vulkan/RenderTarget.h"

#include "3rd/spirv-reflect/spirv_reflect.h"

namespace api
{

  extern TypedPool<RenderState, 256>     s_oRenderStatePool;
  extern TypedPool<ResourceSet, 256>  s_oResourceSetPool;
  extern TypedPool<ConstantBuffer, 256>  s_oConstantBufferPool;
  extern TypedPool<Mesh, 256>            s_oMeshPool;
  extern TypedPool<Texture, 256>         s_oTexturePool;
  extern TypedPool<RenderTarget, 256>    s_oRenderTargetPool;

  extern TypedContiguousPool<VkBuffer, 256>               s_oVkBufferPool;
  extern TypedContiguousPool<VkDeviceMemory, 256>         s_oVkDeviceMemoryPool;
  extern TypedContiguousPool<void*, 256>                  s_oVoidPtrPool;
  extern TypedContiguousPool<VkDescriptorSet, 256>        s_oVkDescriptorSetPool;  
  extern TypedContiguousPool<VkAttachmentReference, 16>    s_oVkAttachmentReferencePool;
  extern TypedContiguousPool<VkAttachmentDescription, 16>  s_oVkAttachmentDescriptionPool;
  extern TypedContiguousPool<VkImageView, 256>            s_oVkImageViewPool;
  extern TypedContiguousPool<VkPipelineColorBlendAttachmentState, 16> s_oPipelineColorBlendAttachmentPool;

}
