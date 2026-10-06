#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "3rd/spirv-reflect/spirv_reflect.h"

#include "InFile.h"
#include "RenderStateInfo.h"
#include "PipelineStage.h"
#include "BlendEnums.h"
#include "CullMode.h"
#include "DepthCompareOp.h"
#include "ImageFormat.h"
#include "ResourceBindInfo.h"
#include "SamplerConfig.h"
#include "API/Vulkan/VulkanData.h"
#include "API/Vulkan/Mesh.h"
#include "API/Vulkan/ConstantBuffer.h"
#include "API/Vulkan/RenderState.h"
#include "API/Vulkan/ResourceSet.h"
#include "API/Vulkan/Texture.h"
#include "API/Vulkan/RenderTarget.h"
#include "API/Vulkan/DescriptorUtils.h"

struct GLFWwindow;

namespace api
{
  extern VulkanData s_oGlobalData;

  VkFormat GetVKFormat(ImageFormat _eFormat);

  VkShaderStageFlags GetVkShaderFlags(PipelineStageFlags _eStageFlags);

  VkBlendOp GetVkBlendOp(BlendOp _eBlendOp);

  VkBlendFactor GetVkBlendFactor(BlendFactor _eBlendFactor);

  VkImageUsageFlags GetVkTextureUsage(uint32_t _uUsage);

  VkSamplerAddressMode GetVkAddressMode(TextureAddressMode _AddressMode);

  VkSamplerMipmapMode GetSamplerMipmapMode(TextureFilterMode _eMipmapMode);

  VkFilter GetVkFilter(TextureFilterMode _eFliterMode);

  VkCullModeFlagBits GetVkCullMode(CullMode _eCullMode);

  VkCompareOp GetVkCompareOp(DepthCompareOp _eCompareOp);

  void CreateInstance();

  VkSampleCountFlagBits GetMaxMSAASampleCount();  

  void CreatePhysicalDevice();

  void CreatePipeline(const file::InFile& _oVSFile, const file::InFile& _oPSFile, const file::InFile* _pGSFile, const RenderStateInfo& _oInfo, VkRenderPass _hRenderPass, VkSampleCountFlagBits _uMsaaSamples, uint32_t _uAttachmentCount, RenderState* _pRenderState_);

  void CreateLogicalDevice();

  void RetrieveQueues();

  void CreateSurface();

  void CreateColorBuffer();

  void CreateDepthBuffer();

  void CreateSwapchain();

  void CreateRenderPass(uint32_t _uNumColorTextures, VkFormat _eColorFormat, bool _bHasDepthStencil, VkFormat _eDepthStencilFormat, uint32_t _uMsaaSampleCount, VkRenderPass& hRenderPass_, bool _bOffscreen, bool _bMultiview = false);  

  void CreateCommandBuffers();

  void CreateSyncObjects();

  void CreateDescriptorPool();

  void DestroySwapchain();

  void RecreateSwapchain();

  uint32_t FindMemoryType(uint32_t _uTypeFilter, VkMemoryPropertyFlags _uProperties);

  void CreateBuffer(size_t _uSize, VkBufferUsageFlags _uUsage, VkMemoryPropertyFlags _uProperties, VkBuffer& hBuffer_, VkDeviceMemory& hDeviceMemory_);

  void CreateImage(uint32_t _uWidth, uint32_t _uHeight, VkFormat _eFormat, uint32_t _uMipLevels, uint32_t _uLayers, VkSampleCountFlagBits _eMsaaSampleCount, VkImageTiling _eTiling, VkImageUsageFlags _uUsage, VkImageCreateFlags _uCreateFlags, VkMemoryPropertyFlags _uProperties, VkImage& hImage_, VkDeviceMemory& hMemory_);

  void CreateImageView(VkImage _hImage, VkFormat _eFormat, uint32_t _uBaseMip, uint32_t _uMipLevels, uint32_t _uBaseLayer, uint32_t _uLayers, VkImageViewType _eViewType, VkImageAspectFlags _uAspectFlags, VkImageView& hImageView_);

  void CreateTextureSampler(Texture* _pTexture, uint32_t _uMipLevels, const SamplerConfig& _rSamplerConfig);

  void DestroyBuffer(VkBuffer _hBuffer, VkDeviceMemory _hDeviceMemory_);

  void SetBufferData(const void* _pData, size_t _uSize, VkDeviceMemory& hDeviceMemory_);

  VkCommandBuffer BeginTempCmdBuffer();

  void EndTempCmdBuffer(VkCommandBuffer _hCmdBuffer);

  void CopyBuffer(VkCommandBuffer _hCmdBuffer, VkBuffer _hSrcBuffer, VkBuffer _hDstBuffer, VkDeviceSize _uSize);

  void CopyBufferToImage(VkCommandBuffer _hCmdBuffer, VkBuffer _hBuffer, VkImage _hImage, uint32_t _uWidth, uint32_t _uHeight, uint32_t _uLayers);

  void TransitionImageLayout(VkCommandBuffer _hCmdBuffer, VkImage _hImage, VkFormat _eFormat, uint32_t _uBaseMip, uint32_t _uMipLevels, uint32_t _uBaseLayer, uint32_t _uLayers, VkImageAspectFlags _uAspectFlags, VkImageLayout _eOldLayout, VkImageLayout _eNewLayout);

  void TransitionTextureImageLayout(VkCommandBuffer _hCmdBuffer, Texture* _pTexture, uint32_t _uBaseMip, uint32_t _uMipLevels, uint32_t _uBaseLayer, uint32_t _uLayers, VkImageAspectFlags _uAspectFlags, VkImageLayout _eNewLayout, bool _bForce = false);

  void SetTextureLayoutAllMipLevels(Texture* _pTexture, VkImageLayout _eNewLayout);
  
  void GenerateMipmaps(VkCommandBuffer _hCmdBuffer, VkImage _hImage, int32_t _iWidth, int32_t _iHeight, uint32_t _uMipLevels, uint32_t _uLayers);

  void GetDescSetReflection(const RenderState* _pRenderState, PipelineStage _eStage, SpvReflectDescriptorSet* aDescSets_[4], uint32_t& uDescSetCount_);

  void AddBufferToDescSetUpdater(ConstantBuffer* _pCBuffer, size_t _uSize, uint32_t uBinding, PipelineStageFlags _uStageFlags);

  void AddImageToDescSetUpdater(Texture* _pTexture, uint32_t uBinding, PipelineStageFlags _uStageFlags);

  void SetupImGui();

} // namespace api