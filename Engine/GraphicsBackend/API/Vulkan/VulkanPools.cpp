#include "API/Vulkan/VulkanPools.h"

namespace api
{
  TypedPool<RenderState, 256>     s_oRenderStatePool;
  TypedPool<ResourceSet, 256>  s_oResourceSetPool;
  TypedPool<ConstantBuffer, 256>  s_oConstantBufferPool;
  TypedPool<Mesh, 256>            s_oMeshPool;
  TypedPool<Texture, 256>         s_oTexturePool;
  TypedPool<RenderTarget, 256>    s_oRenderTargetPool; 

  TypedContiguousPool<VkBuffer, 256>               s_oVkBufferPool;
  TypedContiguousPool<VkDeviceMemory, 256>         s_oVkDeviceMemoryPool;
  TypedContiguousPool<void*, 256>                  s_oVoidPtrPool;
  TypedContiguousPool<VkDescriptorSet, 256>        s_oVkDescriptorSetPool;
  TypedContiguousPool<VkAttachmentReference, 16>    s_oVkAttachmentReferencePool;
  TypedContiguousPool<VkAttachmentDescription, 16>  s_oVkAttachmentDescriptionPool;
  TypedContiguousPool<VkImageView, 256>            s_oVkImageViewPool;
  TypedContiguousPool<VkPipelineColorBlendAttachmentState, 16> s_oPipelineColorBlendAttachmentPool;

}