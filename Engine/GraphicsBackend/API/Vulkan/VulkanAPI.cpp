
#include <vector>
#include <string>
#include <cmath>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "3rd/spirv-reflect/spirv_reflect.h"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_vulkan.h"

#include "API/GraphicsAPI.h"

#include "Exception.h"
#include "API/Vulkan/Window.h"
#include "BlendEnums.h"
#include "API/Vulkan/APIInternal.h"
#include "API/Vulkan/VulkanMacros.h"
#include "API/Vulkan/VulkanShaderReflection.h"
#include "API/Vulkan/VulkanPools.h"
#include "API/Vulkan/ResourceLayout.h"
#include "InFile.h"

namespace api
{

  // General

  void Initialize()
  {

    s_oRenderStatePool.Initialize();
    s_oResourceSetPool.Initialize();
    s_oConstantBufferPool.Initialize();
    s_oMeshPool.Initialize();
    s_oTexturePool.Initialize();
    s_oRenderTargetPool.Initialize();

    s_oVkBufferPool.Initialize();
    s_oVkDeviceMemoryPool.Initialize();
    s_oVoidPtrPool.Initialize();
    s_oVkDescriptorSetPool.Initialize();
    s_oVkAttachmentReferencePool.Initialize();
    s_oVkAttachmentDescriptionPool.Initialize();
    s_oVkImageViewPool.Initialize();
    s_oPipelineColorBlendAttachmentPool.Initialize();

    CreateInstance();
    CreatePhysicalDevice();    

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls    
  }

  void ShutDown()
  {        

    s_oRenderStatePool.Clear();
    s_oResourceSetPool.Clear();
    s_oConstantBufferPool.Clear();
    s_oMeshPool.Clear();
    s_oTexturePool.Clear();
    s_oRenderTargetPool.Clear();

    s_oVkBufferPool.Clear();
    s_oVkDeviceMemoryPool.Clear();
    s_oVoidPtrPool.Clear();
    s_oVkDescriptorSetPool.Clear();
    s_oVkAttachmentReferencePool.Clear();
    s_oVkAttachmentDescriptionPool.Clear();

    vkDestroyInstance(s_oGlobalData.m_hInstance, NULL);
  }

  uint32_t GetMaxMsaaSamples()
  {
    return static_cast<uint32_t>(s_oGlobalData.m_eMaxMsaaSampleCount);
  }

  // Window

  void CreateAPIWindow(HWND _hwnd, uint32_t _uMsaaSamples)
  {
    bool bMsaa = _uMsaaSamples > 1u;

    s_oGlobalData.m_oWindow.m_hwnd = _hwnd;

    s_oGlobalData.m_oWindow.m_eMsaaSamples = static_cast<VkSampleCountFlagBits>(_uMsaaSamples);

    if (_uMsaaSamples > static_cast<uint32_t>(s_oGlobalData.m_eMaxMsaaSampleCount))
    {
      s_oGlobalData.m_oWindow.m_eMsaaSamples = s_oGlobalData.m_eMaxMsaaSampleCount;
    }

    CreateLogicalDevice();
    RetrieveQueues();
    CreateSurface();
    CreateCommandBuffers();
    CreateSwapchain();
    if (bMsaa)
    {
      CreateColorBuffer();
    }
    CreateDepthBuffer();
    CreateSyncObjects();
    CreateDescriptorPool();

    CreateRenderPass(1u, s_oGlobalData.m_oWindow.m_eSwapchainFormat, true, VK_FORMAT_D32_SFLOAT, s_oGlobalData.m_oWindow.m_eMsaaSamples, s_oGlobalData.m_oWindow.m_hRenderPass, false);

    s_oGlobalData.m_oWindow.m_pFramebuffers = new VkFramebuffer[s_oGlobalData.m_oWindow.m_uSwapchainImageCount];

    for (int i = 0; i < s_oGlobalData.m_oWindow.m_uSwapchainImageCount; i++)
    {

      uint32_t uAttachmentCount = 0u;

      VkImageView aImageViews[3];
      aImageViews[uAttachmentCount++] = bMsaa ? s_oGlobalData.m_oWindow.m_hColorImageView : s_oGlobalData.m_oWindow.m_pSwapChainImageViews[i];
      if (bMsaa)
      {
        aImageViews[uAttachmentCount++] = s_oGlobalData.m_oWindow.m_pSwapChainImageViews[i];
      }
      aImageViews[uAttachmentCount++] = s_oGlobalData.m_oWindow.m_hDepthImageView;
      

      VkFramebufferCreateInfo oFramebufferInfo = {};
      oFramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      oFramebufferInfo.attachmentCount = uAttachmentCount;
      oFramebufferInfo.pAttachments = &aImageViews[0];
      oFramebufferInfo.renderPass = s_oGlobalData.m_oWindow.m_hRenderPass;
      oFramebufferInfo.width = s_oGlobalData.m_oWindow.m_oExtent.width;
      oFramebufferInfo.height = s_oGlobalData.m_oWindow.m_oExtent.height;
      oFramebufferInfo.layers = 1;

      VK_CHECK(vkCreateFramebuffer(s_oGlobalData.m_oWindow.m_hDevice, &oFramebufferInfo, NULL, &s_oGlobalData.m_oWindow.m_pFramebuffers[i]))
    }    

    s_oGlobalData.m_oWindow.m_uCurrFrameIdx = 0u;

    // Create dummy texture
    SamplerConfig oDummySamplerConfig = {};
    uint8_t aDummyBuffer[4] = { 255u, 20u, 0u, 255u};    
    void* pDummyBuffer = &(aDummyBuffer[0]);
    s_oGlobalData.m_oWindow.m_pDummyTexture = CreateTexture(&pDummyBuffer, 1u, 1u, ImageFormat::R8G8B8A8, 1u, 1u, TextureUsage::SHADER_RESOURCE, oDummySamplerConfig, false);

    // ImGui stuff
    SetupImGui();
  }  

  void OnWindowResize()
  {
    s_oGlobalData.m_oWindow.m_bResized = true;
  }

  uint32_t GetWindowWidth()
  {
    return s_oGlobalData.m_oWindow.m_oExtent.width;
  }

  uint32_t GetWindowHeight()
  {
    return s_oGlobalData.m_oWindow.m_oExtent.height;
  }

  uint32_t GetWindowMSAASamples()
  {
    return s_oGlobalData.m_oWindow.m_eMsaaSamples;
  }

  void ClearDefaultRenderTarget()
  {
    uint32_t uFrameIdx = s_oGlobalData.m_oWindow.m_uCurrFrameIdx;

    VkCommandBuffer hCmdBuffer = s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx];

    // Depth buffer

    TransitionImageLayout(hCmdBuffer
      , s_oGlobalData.m_oWindow.m_hDepthImage
      , VK_FORMAT_D32_SFLOAT
      , 0u, 1u
      , 0u, 1u
      , VK_IMAGE_ASPECT_DEPTH_BIT
      , VK_IMAGE_LAYOUT_UNDEFINED
      , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageSubresourceRange oRange = {};
    oRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    oRange.baseMipLevel = 0;
    oRange.levelCount = VK_REMAINING_MIP_LEVELS;
    oRange.baseArrayLayer = 0;
    oRange.layerCount = 1;
    
    VkClearDepthStencilValue oDepthClearColor = { 1.f, 0.f };
    vkCmdClearDepthStencilImage(hCmdBuffer, s_oGlobalData.m_oWindow.m_hDepthImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &oDepthClearColor, 1u, &oRange);

    TransitionImageLayout(hCmdBuffer
      , s_oGlobalData.m_oWindow.m_hDepthImage
      , VK_FORMAT_D32_SFLOAT
      , 0u, 1u
      , 0u, 1u
      , VK_IMAGE_ASPECT_DEPTH_BIT
      , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
      , VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    // Color buffer

    if (s_oGlobalData.m_oWindow.m_hColorImage != VK_NULL_HANDLE)
    {
      TransitionImageLayout(hCmdBuffer
        , s_oGlobalData.m_oWindow.m_hColorImage
        , s_oGlobalData.m_oWindow.m_eSwapchainFormat
        , 0u, 1u
        , 0u, 1u
        , VK_IMAGE_ASPECT_COLOR_BIT
        , VK_IMAGE_LAYOUT_UNDEFINED
        , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      oRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      oRange.baseMipLevel = 0;
      oRange.levelCount = VK_REMAINING_MIP_LEVELS;
      oRange.baseArrayLayer = 0;
      oRange.layerCount = 1;

      VkClearColorValue oClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
      vkCmdClearColorImage(hCmdBuffer, s_oGlobalData.m_oWindow.m_hColorImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &oClearColor, 1u, &oRange);

      TransitionImageLayout(hCmdBuffer
        , s_oGlobalData.m_oWindow.m_hColorImage
        , s_oGlobalData.m_oWindow.m_eSwapchainFormat
        , 0u, 1u
        , 0u, 1u
        , VK_IMAGE_ASPECT_COLOR_BIT
        , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        , VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    else
    {      
      TransitionImageLayout(hCmdBuffer
        , s_oGlobalData.m_oWindow.m_pSwapchainImages[s_oGlobalData.m_oWindow.m_uCurrSwapchainImageIdx]
        , s_oGlobalData.m_oWindow.m_eSwapchainFormat
        , 0u, 1u
        , 0u, 1u
        , VK_IMAGE_ASPECT_COLOR_BIT
        , VK_IMAGE_LAYOUT_UNDEFINED
        , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
       
      oRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      oRange.baseMipLevel = 0;
      oRange.levelCount = VK_REMAINING_MIP_LEVELS;
      oRange.baseArrayLayer = 0;
      oRange.layerCount = 1;

      VkClearColorValue oClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
      vkCmdClearColorImage(hCmdBuffer, s_oGlobalData.m_oWindow.m_pSwapchainImages[s_oGlobalData.m_oWindow.m_uCurrSwapchainImageIdx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &oClearColor, 1u, &oRange);

      TransitionImageLayout(hCmdBuffer
        , s_oGlobalData.m_oWindow.m_pSwapchainImages[s_oGlobalData.m_oWindow.m_uCurrSwapchainImageIdx]
        , s_oGlobalData.m_oWindow.m_eSwapchainFormat
        , 0u, 1u
        , 0u, 1u
        , VK_IMAGE_ASPECT_COLOR_BIT
        , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        , VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
  }

  void BindDefaultRenderTarget()
  {    
    VkClearValue aClearColors[3];
    aClearColors[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };    
    aClearColors[1].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    aClearColors[2].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo oRenderPassBeginInfo{};
    oRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    oRenderPassBeginInfo.renderPass = s_oGlobalData.m_oWindow.m_hRenderPass;
    oRenderPassBeginInfo.framebuffer = s_oGlobalData.m_oWindow.m_pFramebuffers[s_oGlobalData.m_oWindow.m_uCurrSwapchainImageIdx];
    oRenderPassBeginInfo.renderArea.extent = s_oGlobalData.m_oWindow.m_oExtent;
    oRenderPassBeginInfo.renderArea.offset = { 0,0 };
    oRenderPassBeginInfo.clearValueCount = 3u;
    oRenderPassBeginInfo.pClearValues = aClearColors;

    const uint32_t uFrameIdx = s_oGlobalData.m_oWindow.m_uCurrFrameIdx;

    vkCmdBeginRenderPass(s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx], &oRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);    

    VkViewport oViewport{};
    oViewport.x = 0.0f;
    oViewport.y = (float)s_oGlobalData.m_oWindow.m_oExtent.height;
    oViewport.width = (float)s_oGlobalData.m_oWindow.m_oExtent.width;
    oViewport.height = -(float)s_oGlobalData.m_oWindow.m_oExtent.height;
    oViewport.minDepth = 0.0f;
    oViewport.maxDepth = 1.0f;

    vkCmdSetViewport(s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx], 0, 1, &oViewport);

    VkRect2D oScissor{};
    oScissor.offset = { 0, 0 };
    oScissor.extent = s_oGlobalData.m_oWindow.m_oExtent;

    vkCmdSetScissor(s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx], 0, 1, &oScissor);
  }

  void UnbindDefaultRenderTarget()
  {
    
    const uint32_t uFrameIdx = s_oGlobalData.m_oWindow.m_uCurrFrameIdx;

    // Record dear imgui primitives into command buffer
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx]);

    vkCmdEndRenderPass(s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx]);    
  }

  void DestroyWindow()
  {

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    const uint32_t uNumImages = s_oGlobalData.m_oWindow.m_uSwapchainImageCount;

    VK_CHECK(vkWaitForFences(s_oGlobalData.m_oWindow.m_hDevice, uNumImages, s_oGlobalData.m_oWindow.m_pInFlightFences, VK_TRUE, UINT64_MAX))

    DestroyTexture(s_oGlobalData.m_oWindow.m_pDummyTexture);

    vkDestroyRenderPass(s_oGlobalData.m_oWindow.m_hDevice, s_oGlobalData.m_oWindow.m_hRenderPass, NULL);

    DestroySwapchain();

    delete[] s_oGlobalData.m_oWindow.m_pFramebuffers;

    for (int i = 0; i < uNumImages; i++)
    {
      vkDestroySemaphore(s_oGlobalData.m_oWindow.m_hDevice, s_oGlobalData.m_oWindow.m_pImageAvailableSemaphores[i], NULL);
      vkDestroySemaphore(s_oGlobalData.m_oWindow.m_hDevice, s_oGlobalData.m_oWindow.m_pRenderFinishedSemaphores[i], NULL);
      vkDestroyFence(s_oGlobalData.m_oWindow.m_hDevice, s_oGlobalData.m_oWindow.m_pInFlightFences[i], NULL);
    }

    delete[] s_oGlobalData.m_oWindow.m_pImageAvailableSemaphores;
    delete[] s_oGlobalData.m_oWindow.m_pRenderFinishedSemaphores;
    delete[] s_oGlobalData.m_oWindow.m_pInFlightFences;

    vkDestroyDescriptorPool(s_oGlobalData.m_oWindow.m_hDevice, s_oGlobalData.m_oWindow.m_hDescPool, NULL);        

    vkDestroyCommandPool(s_oGlobalData.m_oWindow.m_hDevice, s_oGlobalData.m_oWindow.m_hCmdPool, NULL);

    delete[] s_oGlobalData.m_oWindow.m_pCmdBuffers;    

    // Color/depth images and their views are destroyed by DestroySwapchain()
    // to avoid double-destroying Vulkan objects here.

    vkDestroySurfaceKHR(s_oGlobalData.m_hInstance, s_oGlobalData.m_oWindow.m_hSurface, NULL);    

    vkDestroyDevice(s_oGlobalData.m_oWindow.m_hDevice, NULL); 
  }

  // Mesh

  Mesh* CreateMesh( const void* _pVertexData, size_t _uVertexDataSize, const void* _pIndexData, size_t _uIndexDataSize, size_t /*_uMeshConstantSize*/)
  {
    Mesh* pMesh = s_oMeshPool.PullElement();

    VkCommandBuffer hCmdBuffer = BeginTempCmdBuffer();

    VkBuffer hVertexStagingBuffer;
    VkDeviceMemory hVertexStagingBufferMemory;
    VkBuffer hIndexStagingBuffer;
    VkDeviceMemory hIndexStagingBufferMemory;

    // Create vertex buffer
    {      
      VkMemoryPropertyFlags uProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
      VkBufferUsageFlags uUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

      CreateBuffer(_uVertexDataSize, uUsage, uProperties, hVertexStagingBuffer, hVertexStagingBufferMemory);
      SetBufferData(_pVertexData, _uVertexDataSize, hVertexStagingBufferMemory);

      uProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      uUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

      CreateBuffer(_uVertexDataSize, uUsage, uProperties, pMesh->m_hVertexBuffer, pMesh->m_hVertexBufferMemory);

      CopyBuffer(hCmdBuffer, hVertexStagingBuffer, pMesh->m_hVertexBuffer, _uVertexDataSize);      
    }

    // Create index buffer
    {
      VkMemoryPropertyFlags uProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
      VkBufferUsageFlags uUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

      CreateBuffer(_uIndexDataSize, uUsage, uProperties, hIndexStagingBuffer, hIndexStagingBufferMemory);
      SetBufferData(_pIndexData, _uIndexDataSize, hIndexStagingBufferMemory);

      uProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      uUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

      CreateBuffer(_uIndexDataSize, uUsage, uProperties, pMesh->m_hIndexBuffer, pMesh->m_hIndexBufferMemory);

      CopyBuffer(hCmdBuffer, hIndexStagingBuffer, pMesh->m_hIndexBuffer, _uIndexDataSize);      
    }

    EndTempCmdBuffer(hCmdBuffer);

    vkDestroyBuffer(s_oGlobalData.m_oWindow.m_hDevice, hVertexStagingBuffer, NULL);
    vkFreeMemory(s_oGlobalData.m_oWindow.m_hDevice, hVertexStagingBufferMemory, NULL);
    vkDestroyBuffer(s_oGlobalData.m_oWindow.m_hDevice, hIndexStagingBuffer, NULL);
    vkFreeMemory(s_oGlobalData.m_oWindow.m_hDevice, hIndexStagingBufferMemory, NULL);

    return pMesh;
  }

  void DestroyMesh(Mesh* _pMesh)
  {

    const uint32_t uNumImages = s_oGlobalData.m_oWindow.m_uSwapchainImageCount;

    VK_CHECK(vkWaitForFences(s_oGlobalData.m_oWindow.m_hDevice, uNumImages, s_oGlobalData.m_oWindow.m_pInFlightFences, VK_TRUE, UINT64_MAX))

    DestroyBuffer(_pMesh->m_hVertexBuffer, _pMesh->m_hVertexBufferMemory);
    DestroyBuffer(_pMesh->m_hIndexBuffer, _pMesh->m_hIndexBufferMemory);

    s_oMeshPool.ReturnElement(_pMesh);
  }

  // Constant buffer

  ConstantBuffer* CreateConstantBuffer(size_t _uSize)
  {
    ConstantBuffer* pCBuffer = s_oConstantBufferPool.PullElement();        
    
    const uint32_t uNumImages = s_oGlobalData.m_oWindow.m_uSwapchainImageCount;

    // Create buffers

    pCBuffer->m_pUniformBuffers = s_oVkBufferPool.PullElements(uNumImages);
    pCBuffer->m_pUniformBuffersMemory = s_oVkDeviceMemoryPool.PullElements(uNumImages);
    pCBuffer->m_pUniformBuffersMapped = s_oVoidPtrPool.PullElements(uNumImages);

    for (uint32_t i = 0; i < uNumImages; i++)
    {
      CreateBuffer(_uSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        pCBuffer->m_pUniformBuffers[i],
        pCBuffer->m_pUniformBuffersMemory[i]);

      VK_CHECK(vkMapMemory(s_oGlobalData.m_oWindow.m_hDevice, pCBuffer->m_pUniformBuffersMemory[i], 0u, _uSize, 0u, &pCBuffer->m_pUniformBuffersMapped[i]))
    }

    return pCBuffer;
  }

  void UpdateConstantBuffer(ConstantBuffer* _pCBuffer, const void* _pData, size_t _uSize)
  {    
    if (s_oGlobalData.m_bRenderBegan)
    {
      memcpy(_pCBuffer->m_pUniformBuffersMapped[s_oGlobalData.m_oWindow.m_uCurrFrameIdx], _pData, _uSize);
    }
    else
    {
      for(uint32_t i = 0; i < s_oGlobalData.m_oWindow.m_uSwapchainImageCount; i++)
      {
        memcpy(_pCBuffer->m_pUniformBuffersMapped[i], _pData, _uSize);
      }
    }
  }

  void DestroyConstantBuffer(ConstantBuffer* _pCBuffer)
  {       
    const uint32_t uNumImages = s_oGlobalData.m_oWindow.m_uSwapchainImageCount;

    VK_CHECK(vkWaitForFences(s_oGlobalData.m_oWindow.m_hDevice, uNumImages, s_oGlobalData.m_oWindow.m_pInFlightFences, VK_TRUE, UINT64_MAX))    

    for (uint32_t i = 0; i < uNumImages; i++)
    {
      vkUnmapMemory(s_oGlobalData.m_oWindow.m_hDevice, _pCBuffer->m_pUniformBuffersMemory[i]);
      DestroyBuffer(_pCBuffer->m_pUniformBuffers[i], _pCBuffer->m_pUniformBuffersMemory[i]);
    }

    s_oVkBufferPool.ReturnElements(_pCBuffer->m_pUniformBuffers);
    s_oVkDeviceMemoryPool.ReturnElements(_pCBuffer->m_pUniformBuffersMemory);
    s_oVoidPtrPool.ReturnElements(_pCBuffer->m_pUniformBuffersMapped);

    s_oConstantBufferPool.ReturnElement(_pCBuffer);
  }

  // Texture

  Texture* CreateTexture(const void* const* _ppData, uint32_t _uWidth, uint32_t _uHeight, ImageFormat _eFormat, uint32_t _uMipLevels, uint32_t _uMsaaSamples, uint32_t _uUsage, const SamplerConfig& _rSamplerConfig, bool _bIsCubemap)
  {

    Texture* pTexture = s_oTexturePool.PullElement();

    VkBuffer hStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory hStagingBufferMemory;       

    size_t uSize = _uWidth * _uHeight * GetImageFormatSize(_eFormat);

    if (_bIsCubemap)
    {
      uSize *= 6u;
    }

    pTexture->m_eFormat = GetVKFormat(_eFormat);

    uint32_t uMaxMipLevels = static_cast<uint32_t>(std::floor(std::log2(max(_uWidth, _uWidth)))) + 1;
    _uMipLevels = _uMipLevels == 0u? uMaxMipLevels : min(_uMipLevels, uMaxMipLevels);

    uint32_t uLayers = _bIsCubemap ? 6u : 1u;

    pTexture->m_uMipLevels = _uMipLevels;
    pTexture->m_uLayers = uLayers;
    pTexture->m_uWidth = _uWidth;
    pTexture->m_uHeight = _uHeight;

    if (_ppData != nullptr)
    {
      CreateBuffer(
          uSize
        , VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        , hStagingBuffer
        , hStagingBufferMemory);

      if (_bIsCubemap)
      {
        size_t uSizePerLayer = uSize / 6u;
        void* pDeviceData;
        VK_CHECK(vkMapMemory(s_oGlobalData.m_oWindow.m_hDevice, hStagingBufferMemory, 0, uSize, 0, &pDeviceData))
        for (size_t i = 0u; i < 6u; i++)
        {          
          memcpy(static_cast<char*>(pDeviceData) + uSizePerLayer * i, *(_ppData + i), uSizePerLayer);
        }
        vkUnmapMemory(s_oGlobalData.m_oWindow.m_hDevice, hStagingBufferMemory);
      }
      else
      {
        SetBufferData(*_ppData, uSize, hStagingBufferMemory);
      }
    }

    VkImageUsageFlags uUsageFlags = GetVkTextureUsage(_uUsage);

    if (_ppData != nullptr)
    {
      uUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (_uMipLevels > 1u)
    {
      uUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    /*if (_uMsaaSamples > 1u)
    {
      uUsageFlags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    }*/

    CreateImage(
        _uWidth
      , _uHeight
      , pTexture->m_eFormat
      , _uMipLevels
      , uLayers
      , static_cast<VkSampleCountFlagBits>(_uMsaaSamples)
      , VK_IMAGE_TILING_OPTIMAL
      , uUsageFlags
      , _bIsCubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u      
      , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT      
      , pTexture->m_hImage
      , pTexture->m_hMemory);

    uint32_t uAspectFlags = 0u;

    if ((_uUsage & TextureUsage::DEPTH_TARGET) != 0u)
    {
      uAspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    else
    {
      uAspectFlags |= VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkCommandBuffer hCmdBuffer = BeginTempCmdBuffer();

    if (_ppData != nullptr)
    {
      TransitionImageLayout(hCmdBuffer
        , pTexture->m_hImage
        , pTexture->m_eFormat
        , 0u, _uMipLevels
        , 0u, uLayers
        , uAspectFlags
        , VK_IMAGE_LAYOUT_UNDEFINED
        , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      CopyBufferToImage(hCmdBuffer, hStagingBuffer, pTexture->m_hImage, _uWidth, _uHeight, uLayers);


      // When we generate mipmaps, we also do the transition layout
      if (_uMipLevels > 1u)
      {
        GenerateMipmaps(hCmdBuffer, pTexture->m_hImage, _uWidth, _uHeight, _uMipLevels, uLayers);
      }
      // If no mipmaps, perform layout transition
      else
      {
        TransitionImageLayout(hCmdBuffer
          , pTexture->m_hImage
          , pTexture->m_eFormat
          , 0u, _uMipLevels
          , 0u, uLayers
          , uAspectFlags
          , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
          , VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      }

      SetTextureLayoutAllMipLevels(pTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    else
    {

      VkImageLayout eFinalLayout = ((_uUsage & TextureUsage::COLOR_TARGET) != 0)
        ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

      TransitionImageLayout(hCmdBuffer
        , pTexture->m_hImage
        , pTexture->m_eFormat
        , 0u, _uMipLevels
        , 0u, uLayers
        , uAspectFlags
        , VK_IMAGE_LAYOUT_UNDEFINED
        , eFinalLayout);
      
      SetTextureLayoutAllMipLevels(pTexture, eFinalLayout);
    }

    EndTempCmdBuffer(hCmdBuffer);

    if (hStagingBuffer != VK_NULL_HANDLE)
    {
      DestroyBuffer(hStagingBuffer, hStagingBufferMemory);
    }

    CreateImageView(
        pTexture->m_hImage
      , pTexture->m_eFormat
      , 0u, _uMipLevels
      , 0u, uLayers
      , _bIsCubemap ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D
      , uAspectFlags
      , pTexture->m_hImageView);

    if ((_uUsage & TextureUsage::SHADER_RESOURCE) != 0u)
    {
      CreateTextureSampler(pTexture, _uMipLevels, _rSamplerConfig);
    }      

    return pTexture;
  }

  void GenerateMipMaps(Texture* _pTexture)
  {
    if (_pTexture->m_uMipLevels > 1u)
    {            
      VkCommandBuffer hCmdBuffer = s_oGlobalData.m_oWindow.m_pCmdBuffers[s_oGlobalData.m_oWindow.m_uCurrFrameIdx];

      TransitionTextureImageLayout(hCmdBuffer
        , _pTexture
        , 0u, _pTexture->m_uMipLevels
        , 0u, _pTexture->m_uLayers
        , VK_IMAGE_ASPECT_COLOR_BIT
        , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      GenerateMipmaps(hCmdBuffer, _pTexture->m_hImage, _pTexture->m_uWidth, _pTexture->m_uHeight, _pTexture->m_uMipLevels, _pTexture->m_uLayers);
      
      SetTextureLayoutAllMipLevels(_pTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
  }

  void BindTexture(Texture* _pTexture)
  {    
    uint32_t uFrameIdx = s_oGlobalData.m_oWindow.m_uCurrFrameIdx;

    TransitionTextureImageLayout(
      s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx]
      , _pTexture
      , 0u, _pTexture->m_uMipLevels
      , 0u, _pTexture->m_uLayers
      , VK_IMAGE_ASPECT_COLOR_BIT
      , VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  void UnbindTexture(Texture* /*_pTexture*/)
  {

  }

  void ClearTexture(Texture* _pTexture, TextureUsage _eUsage)
  {    

    uint32_t uFrameIdx = s_oGlobalData.m_oWindow.m_uCurrFrameIdx;

    uint32_t uAspectFlags = 0u;

    switch (_eUsage)
    {
    case TextureUsage::DEPTH_TARGET:
      uAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
      break;
    case TextureUsage::COLOR_TARGET:
      uAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
      break;
    default:
      THROW_GENERIC_EXCEPTION("[API] [VK] Invalid usage flag in ClearAPITexture")
      break;
    }

    VkCommandBuffer hCmdBuffer = s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx];

    TransitionTextureImageLayout(hCmdBuffer,
      _pTexture,
      0u, _pTexture->m_uMipLevels, 
      0u, 1u, 
      uAspectFlags, 
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageSubresourceRange oRange = {};
    oRange.aspectMask = uAspectFlags;
    oRange.baseMipLevel = 0;
    oRange.levelCount = VK_REMAINING_MIP_LEVELS;
    oRange.baseArrayLayer = 0;
    oRange.layerCount = 1;

    if (_eUsage == TextureUsage::DEPTH_TARGET)
    {
      VkClearDepthStencilValue oClearColor = { 1.f, 0.f };
      vkCmdClearDepthStencilImage(s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx], 
        _pTexture->m_hImage, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        &oClearColor, 
        1u, 
        &oRange);
    }
    else
    {
      VkClearColorValue oClearColor = { 0.0f, 0.0f, 0.0f, 1.0f }; 
      vkCmdClearColorImage(hCmdBuffer, 
        _pTexture->m_hImage, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        &oClearColor, 
        1u, 
        &oRange);
    }

    VkImageLayout eFinalLayout = _eUsage == TextureUsage::COLOR_TARGET 
      ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
      : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    TransitionTextureImageLayout(hCmdBuffer,
      _pTexture,
      0u, _pTexture->m_uMipLevels,
      0u, 1u,
      uAspectFlags,
      eFinalLayout);    
  }

  void BlitTexture(Texture* _pSrcTexture, const TextureSubResource& _rSrcSubResource, Texture* _pDstTexture, const TextureSubResource& _rDstSubResource)
  {    

    if (_rSrcSubResource.m_uMipLevels != _rDstSubResource.m_uMipLevels || _rSrcSubResource.m_uLayers != _rDstSubResource.m_uLayers)
    {
      THROW_GENERIC_EXCEPTION("BlitAPITexture: Subresource Mip level count and layer count must match");
    }
    else if ((_rSrcSubResource.m_uBaseMip + _rSrcSubResource.m_uMipLevels) > _pSrcTexture->m_uMipLevels &&
      (_rDstSubResource.m_uBaseMip + _rDstSubResource.m_uMipLevels) > _pDstTexture->m_uMipLevels)
    {
      THROW_GENERIC_EXCEPTION("BlitAPITexture: Subresource Mip levels are outside of the texure mip level range");
    }
    else if ((_rSrcSubResource.m_uBaseLayer + _rSrcSubResource.m_uLayers) > _pSrcTexture->m_uLayers &&
      (_rDstSubResource.m_uBaseLayer + _rDstSubResource.m_uLayers) > _pDstTexture->m_uLayers)
    {
      THROW_GENERIC_EXCEPTION("BlitAPITexture: Subresource array layers are outside of the texure array layers range");
    }

    VkCommandBuffer& hCmdBuffer = s_oGlobalData.m_oWindow.m_pCmdBuffers[s_oGlobalData.m_oWindow.m_uCurrFrameIdx];

    VkImageLayout eSrcOldLayout = _pSrcTexture->m_aCurrLayouts[0];
    VkImageLayout eDstOldLayout = _pDstTexture->m_aCurrLayouts[0];

    TransitionTextureImageLayout(hCmdBuffer
      , _pSrcTexture      
      , _rSrcSubResource.m_uBaseMip
      , _rSrcSubResource.m_uMipLevels
      , _rSrcSubResource.m_uBaseLayer
      , _rSrcSubResource.m_uLayers
      , VK_IMAGE_ASPECT_COLOR_BIT
      , VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    TransitionTextureImageLayout(hCmdBuffer
      , _pDstTexture
      , _rSrcSubResource.m_uBaseMip
      , _rSrcSubResource.m_uMipLevels
      , _rSrcSubResource.m_uBaseLayer
      , _rSrcSubResource.m_uLayers
      , VK_IMAGE_ASPECT_COLOR_BIT
      , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageBlit oBlit{};
    oBlit.srcOffsets[0] = { 0, 0, 0 };
    oBlit.srcOffsets[1] = { static_cast<int>(_pSrcTexture->m_uWidth), static_cast<int>(_pSrcTexture->m_uHeight), 1 };
    oBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    oBlit.srcSubresource.mipLevel = _rSrcSubResource.m_uBaseMip;    
    oBlit.srcSubresource.baseArrayLayer = _rSrcSubResource.m_uBaseLayer;
    oBlit.srcSubresource.layerCount = _rSrcSubResource.m_uLayers;
    oBlit.dstOffsets[0] = { 0, 0, 0 };
    oBlit.dstOffsets[1] = { static_cast<int>(_pDstTexture->m_uWidth), static_cast<int>(_pDstTexture->m_uHeight), 1 };
    oBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    oBlit.dstSubresource.mipLevel = _rDstSubResource.m_uBaseMip;
    oBlit.dstSubresource.baseArrayLayer = _rDstSubResource.m_uBaseLayer;
    oBlit.dstSubresource.layerCount = _rDstSubResource.m_uLayers;

    vkCmdBlitImage(hCmdBuffer,
      _pSrcTexture->m_hImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      _pDstTexture->m_hImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1u, &oBlit,
      VK_FILTER_LINEAR);

    TransitionTextureImageLayout(hCmdBuffer
      , _pSrcTexture
      , _rSrcSubResource.m_uBaseMip
      , _rSrcSubResource.m_uMipLevels
      , _rSrcSubResource.m_uBaseLayer
      , _rSrcSubResource.m_uLayers
      , VK_IMAGE_ASPECT_COLOR_BIT
      , eSrcOldLayout);

    TransitionTextureImageLayout(hCmdBuffer
      , _pDstTexture
      , _rSrcSubResource.m_uBaseMip
      , _rSrcSubResource.m_uMipLevels
      , _rSrcSubResource.m_uBaseLayer
      , _rSrcSubResource.m_uLayers
      , VK_IMAGE_ASPECT_COLOR_BIT
      , eDstOldLayout);
  }

  void DestroyTexture(Texture* _pTexture)
  {
    const uint32_t uNumImages = s_oGlobalData.m_oWindow.m_uSwapchainImageCount;

    VK_CHECK(vkWaitForFences(s_oGlobalData.m_oWindow.m_hDevice, uNumImages, s_oGlobalData.m_oWindow.m_pInFlightFences, VK_TRUE, UINT64_MAX))

    vkDestroySampler(s_oGlobalData.m_oWindow.m_hDevice, _pTexture->m_hSampler, NULL);
    vkDestroyImageView(s_oGlobalData.m_oWindow.m_hDevice, _pTexture->m_hImageView, NULL);
    vkDestroyImage(s_oGlobalData.m_oWindow.m_hDevice, _pTexture->m_hImage, NULL);
    vkFreeMemory(s_oGlobalData.m_oWindow.m_hDevice, _pTexture->m_hMemory, NULL);

    s_oTexturePool.ReturnElement(_pTexture);
  }

  // RenderTarget

  RenderTarget* CreateRenderTarget()
  {
    RenderTarget* pRenderTarget = s_oRenderTargetPool.PullElement();    

    return pRenderTarget;
  }

  void BeginRenderTargetSetup(RenderTarget* _pRenderTarget, uint32_t _uWidth, uint32_t _uHeight, ImageFormat _eFormat, ImageFormat _eDepthStencilFormat, uint32_t _uMsaaSamples, bool _isCubemap)
  {
    s_oGlobalData.m_oRenderTargetBuilder.SetRenderTarget(_pRenderTarget);
    s_oGlobalData.m_oRenderTargetBuilder.m_eColorFormat = GetVKFormat(_eFormat);
    s_oGlobalData.m_oRenderTargetBuilder.m_eDepthStencilFormat = GetVKFormat(_eDepthStencilFormat);
    s_oGlobalData.m_oRenderTargetBuilder.m_uMsaaSamples = _uMsaaSamples;
    s_oGlobalData.m_oRenderTargetBuilder.m_uWidth = _uWidth;
    s_oGlobalData.m_oRenderTargetBuilder.m_uHeight = _uHeight;
  }

  void RenderTargetAddColorTexture(Texture* _pTexture, uint32_t _uMipLevel)
  {
    s_oGlobalData.m_oRenderTargetBuilder.AddColorTexture(_pTexture, _uMipLevel);
  }

  void RenderTargetSetDepthStencilTexture(Texture* _pTexture, uint32_t _uMipLevel)
  {
    s_oGlobalData.m_oRenderTargetBuilder.SetDepthTexture(_pTexture, _uMipLevel);
  }

  void RenderTargetAddColorResolveTexture(Texture* _pTexture)
  {
    s_oGlobalData.m_oRenderTargetBuilder.AddResolveColorTexture(_pTexture);
  }

  void EndRenderTargetSetup()
  {
    s_oGlobalData.m_oRenderTargetBuilder.Build();
    s_oGlobalData.m_oRenderTargetBuilder.Clear();
  }

  void ClearRenderTarget(RenderTarget* /*_pRenderTarget*/)
  {  
  }

  void BindRenderTarget(RenderTarget* _pRenderTarget)
  {
    
    VkCommandBuffer hCmdBuffer = s_oGlobalData.m_oWindow.m_pCmdBuffers[s_oGlobalData.m_oWindow.m_uCurrFrameIdx];

    for (uint32_t i = 0u; i < 4u; i++)
    {      
      if (Texture* pTexture = _pRenderTarget->m_aColorTextures[i])
      {
        TransitionTextureImageLayout(hCmdBuffer
          , pTexture
          , _pRenderTarget->m_aColorTexturesMipLevels[i], 1u
          , 0u, pTexture->m_uLayers
          , VK_IMAGE_ASPECT_COLOR_BIT
          , VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      }
    }

    if (_pRenderTarget->m_pDepthTexture)
    {
      TransitionTextureImageLayout(hCmdBuffer
        , _pRenderTarget->m_pDepthTexture
        , _pRenderTarget->m_uDepthTextureMipLevel, 1u
        , 0u, _pRenderTarget->m_pDepthTexture->m_uLayers
        , VK_IMAGE_ASPECT_DEPTH_BIT
        , VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    VkExtent2D oExtent = {};
    oExtent.width = _pRenderTarget->m_uWidth;
    oExtent.height = _pRenderTarget->m_uHeight;

    VkRenderPassBeginInfo oRenderPassBeginInfo{};
    oRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    oRenderPassBeginInfo.renderPass = _pRenderTarget->m_hRenderPass;
    oRenderPassBeginInfo.framebuffer = _pRenderTarget->m_hFrameBuffer;
    oRenderPassBeginInfo.renderArea.extent = oExtent;
    oRenderPassBeginInfo.renderArea.offset = { 0,0 };
    oRenderPassBeginInfo.clearValueCount = 0u;
    oRenderPassBeginInfo.pClearValues = nullptr;

    vkCmdBeginRenderPass(hCmdBuffer, &oRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport oViewport{};
    oViewport.x = 0.0f;
    oViewport.y = (float)oExtent.height;
    oViewport.width = (float)oExtent.width;
    oViewport.height = -(float)oExtent.height;
    oViewport.minDepth = 0.0f;
    oViewport.maxDepth = 1.0f;

    vkCmdSetViewport(hCmdBuffer, 0, 1, &oViewport);

    VkRect2D oScissor{};
    oScissor.offset = { 0, 0 };
    oScissor.extent = oExtent;

    vkCmdSetScissor(hCmdBuffer, 0, 1, &oScissor);
  }

  void UnbindRenderTarget(RenderTarget* _pRenderTarget)
  {    
    const uint32_t uFrameIdx = s_oGlobalData.m_oWindow.m_uCurrFrameIdx;

    bool bHasMsaa = _pRenderTarget->m_uMsaaSamples > 1u;

    vkCmdEndRenderPass(s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx]);    

    for (uint32_t i = 0u; i < 4u; i++)
    {
      if (_pRenderTarget->m_aColorTextures[i] != nullptr)
      {        
        uint32_t uMipLevel = _pRenderTarget->m_aColorTexturesMipLevels[i];
        _pRenderTarget->m_aColorTextures[i]->m_aCurrLayouts[uMipLevel] = bHasMsaa ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      }

      if (_pRenderTarget->m_aResolveTextures[i] != nullptr)
      {         
        _pRenderTarget->m_aResolveTextures[i]->m_aCurrLayouts[0] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      }
    }

    if (_pRenderTarget->m_pDepthTexture != nullptr)
    {      
      uint32_t uMipLevel = _pRenderTarget->m_uDepthTextureMipLevel;
      _pRenderTarget->m_pDepthTexture->m_aCurrLayouts[uMipLevel] = bHasMsaa ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
  }

  void DestroyRenderTarget(RenderTarget* _pRenderTarget)
  {
    const uint32_t uNumImages = s_oGlobalData.m_oWindow.m_uSwapchainImageCount;

    VK_CHECK(vkWaitForFences(s_oGlobalData.m_oWindow.m_hDevice, uNumImages, s_oGlobalData.m_oWindow.m_pInFlightFences, VK_TRUE, UINT64_MAX))

    vkDestroyFramebuffer(s_oGlobalData.m_oWindow.m_hDevice, _pRenderTarget->m_hFrameBuffer, nullptr);

    vkDestroyRenderPass(s_oGlobalData.m_oWindow.m_hDevice, _pRenderTarget->m_hRenderPass, nullptr);

    if (_pRenderTarget->m_pImageViews)
    {
      for (uint32_t i = 0; i < _pRenderTarget->m_uAttachmentCount; i++)
      {
        vkDestroyImageView(s_oGlobalData.m_oWindow.m_hDevice, _pRenderTarget->m_pImageViews[i], nullptr);
      }
      s_oVkImageViewPool.ReturnElements(_pRenderTarget->m_pImageViews);
    }

    s_oRenderTargetPool.ReturnElement(_pRenderTarget);
  }

  // Render state

  RenderState* CreateRenderState(const RenderStateInfo& _oInfo, const RenderTarget* _pRenderTarget, uint32_t _uMsaaSamples)
  {

    VkRenderPass hRenderPass = VK_NULL_HANDLE;

    if (_pRenderTarget)
    {
      hRenderPass = _pRenderTarget->m_hRenderPass;
    }    
    else
    {
      hRenderPass = s_oGlobalData.m_oWindow.m_hRenderPass;
    }

    RenderState* pRenderState = s_oRenderStatePool.PullElement();

    uint32_t uNumImages = s_oGlobalData.m_oWindow.m_uSwapchainImageCount;    

    file::InFile oVSFile(_oInfo.m_sVSFilename.c_str());
    file::InFile oPSFile(_oInfo.m_sPSFilename.c_str());
    
    SpvReflectResult eResult = spvReflectCreateShaderModule(oVSFile.GetSize(), static_cast<void*>(oVSFile.GetData()), &(pRenderState->m_oVertexReflection));
    if (eResult != SPV_REFLECT_RESULT_SUCCESS)
    {
      THROW_GENERIC_EXCEPTION("[API] Error: Unable to load vertex shader reflection")
    }    

    eResult = spvReflectCreateShaderModule(oPSFile.GetSize(), static_cast<void*>(oPSFile.GetData()), &(pRenderState->m_oPixelReflection));
    if (eResult != SPV_REFLECT_RESULT_SUCCESS)
    {
      THROW_GENERIC_EXCEPTION("[API] Error: Unable to load pixel shader reflection")
    }    
    
    ReflectSetLayouts(pRenderState->m_oVertexReflection, pRenderState->m_aLayoutBuilders);
    ReflectSetLayouts(pRenderState->m_oPixelReflection, pRenderState->m_aLayoutBuilders);

    file::InFile* pGSFile = nullptr;

    if (!_oInfo.m_sGSFilename.empty())
    {      
      pGSFile = new file::InFile(_oInfo.m_sGSFilename.c_str());

      eResult = spvReflectCreateShaderModule(pGSFile->GetSize(), static_cast<void*>(pGSFile->GetData()), &(pRenderState->m_oGeomReflection));
      if (eResult != SPV_REFLECT_RESULT_SUCCESS)
      {
        THROW_GENERIC_EXCEPTION("[API] Error: Unable to load geometry shader reflection")
      }

      ReflectSetLayouts(pRenderState->m_oGeomReflection, pRenderState->m_aLayoutBuilders);
    }      

    for (int i = 0; i < MAX_DESCRIPTOR_SET_LAYOUTS; i++)
    {
      if (pRenderState->m_aLayoutBuilders[i].GetDescriptors().size() > 0u)
      {
        pRenderState->m_aDescriptorSetLayouts[i] = pRenderState->m_aLayoutBuilders[i].Build(s_oGlobalData.m_oWindow.m_hDevice);
      }
      else
      {
        pRenderState->m_aDescriptorSetLayouts[i] = VK_NULL_HANDLE;
      }
    }

    uint32_t uNumAttachments = _pRenderTarget != nullptr ? _pRenderTarget->m_uAttachmentCount : 1u;
    
    CreatePipeline(oVSFile, oPSFile, pGSFile, _oInfo, hRenderPass, static_cast<VkSampleCountFlagBits>(_uMsaaSamples), uNumAttachments, pRenderState);

    if (pGSFile)
    {
      delete pGSFile;
    }
    
    return pRenderState;
  }

  void BindRenderState(RenderState* _pAPIRenderState)
  {
    const uint32_t uFrameIdx = s_oGlobalData.m_oWindow.m_uCurrFrameIdx;

    vkCmdBindPipeline(s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, _pAPIRenderState->m_hGraphicsPipeline);    
  }

  void DestroyRenderState(RenderState* _pAPIRenderState)
  {
    const uint32_t uNumImages = s_oGlobalData.m_oWindow.m_uSwapchainImageCount;

    VK_CHECK(vkWaitForFences(s_oGlobalData.m_oWindow.m_hDevice, uNumImages, s_oGlobalData.m_oWindow.m_pInFlightFences, VK_TRUE, UINT64_MAX))

    spvReflectDestroyShaderModule(&_pAPIRenderState->m_oVertexReflection);    
    spvReflectDestroyShaderModule(&_pAPIRenderState->m_oPixelReflection);
    if (_pAPIRenderState->m_oGeomReflection.entry_point_name != nullptr)
    {
      spvReflectDestroyShaderModule(&_pAPIRenderState->m_oGeomReflection);
    }

    for (int i = 0; i < MAX_DESCRIPTOR_SET_LAYOUTS; i++)
    {
      if (_pAPIRenderState->m_aDescriptorSetLayouts[i] != VK_NULL_HANDLE)
      {
        vkDestroyDescriptorSetLayout(s_oGlobalData.m_oWindow.m_hDevice, _pAPIRenderState->m_aDescriptorSetLayouts[i], NULL);
      }
    }

    vkDestroyPipelineLayout(s_oGlobalData.m_oWindow.m_hDevice, _pAPIRenderState->m_hPipelineLayout, NULL);

    vkDestroyPipeline(s_oGlobalData.m_oWindow.m_hDevice, _pAPIRenderState->m_hGraphicsPipeline, NULL);

    s_oRenderStatePool.ReturnElement(_pAPIRenderState);
  }    

  // SHader reflection

  uint32_t GetConstantBufferCount(const RenderState* _pAPIRenderState, PipelineStage _eStage)
  {
    uint32_t uCBufferCount = 0u;

    uint32_t uDescSetCount = 0u;
    SpvReflectDescriptorSet* aDescSets[4];
    
    GetDescSetReflection(_pAPIRenderState, _eStage, aDescSets, uDescSetCount);

    for (uint32_t i = 0u; i < uDescSetCount; i++)
    {
      SpvReflectDescriptorSet* pDescSet = aDescSets[i];

      //if (pDescSet->set == static_cast<uint32_t>(ResourceFrequency::MATERIAL))
      {
        for (uint32_t j = 0u; j < pDescSet->binding_count; j++)
        {
          SpvReflectDescriptorBinding* pDesc = pDescSet->bindings[j];

          if (pDesc->descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
          {
            uCBufferCount++;
          }
        }
      }
    }

    return uCBufferCount;
  }

  uint32_t GetTextureCount(const RenderState* _pAPIRenderState, PipelineStage _eStage)
  {
    return 0u;
  }

  std::string GetConstantBufferName(const RenderState* _pAPIRenderState, PipelineStage _eStage, uint32_t _uIdx)
  {
    uint32_t uDescSetCount = 0u;
    SpvReflectDescriptorSet* aDescSets[4];

    GetDescSetReflection(_pAPIRenderState, _eStage, aDescSets, uDescSetCount);

    uint32_t uCurrIdx = 0u;

    for (uint32_t i = 0u; i < uDescSetCount; i++)
    {
      SpvReflectDescriptorSet* pDescSet = aDescSets[i];

      //if (pDescSet->set == static_cast<uint32_t>(ResourceFrequency::MATERIAL))
      {
        for (uint32_t j = 0u; j < pDescSet->binding_count; j++)
        {
          SpvReflectDescriptorBinding* pDesc = pDescSet->bindings[j];

          if (pDesc->descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
          {
            if (uCurrIdx == _uIdx)
            {
              return pDesc->block.type_description->type_name;
            }

            uCurrIdx++;
          }
        }
      }
    }

    THROW_GENERIC_EXCEPTION("[API] Constant buffer not found by index");

    return "";
  }

  const char* GetTextureName(const RenderState* _pAPIRenderState, PipelineStage _eStage, uint32_t _uIdx)
  {
    return nullptr;
  }

  uint32_t GetConstantBufferMemberCount(const RenderState* _pAPIRenderState, PipelineStage _eStage, uint32_t _uIdx)
  {
    uint32_t uDescSetCount = 0u;
    SpvReflectDescriptorSet* aDescSets[4];

    GetDescSetReflection(_pAPIRenderState, _eStage, aDescSets, uDescSetCount);

    uint32_t uCurrIdx = 0u;

    for (uint32_t i = 0u; i < uDescSetCount; i++)
    {
      SpvReflectDescriptorSet* pDescSet = aDescSets[i];

      //if (pDescSet->set == static_cast<uint32_t>(ResourceFrequency::MATERIAL))
      {
        for (uint32_t j = 0u; j < pDescSet->binding_count; j++)
        {
          SpvReflectDescriptorBinding* pDesc = pDescSet->bindings[j];

          if (pDesc->descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
          {
            if (uCurrIdx == _uIdx)
            {
              return pDesc->block.member_count;
            }

            uCurrIdx++;
          }
        }
      }
    }

    THROW_GENERIC_EXCEPTION("[API] Constant buffer not found by index");

    return 0u;
  }

  std::string GetConstantBufferMemberName(const RenderState* _pAPIRenderState, PipelineStage _eStage, uint32_t _uIdx, uint32_t _uMemberIdx)
  {
    uint32_t uDescSetCount = 0u;
    SpvReflectDescriptorSet* aDescSets[4];

    GetDescSetReflection(_pAPIRenderState, _eStage, aDescSets, uDescSetCount);

    uint32_t uCurrIdx = 0u;

    for (uint32_t i = 0u; i < uDescSetCount; i++)
    {
      SpvReflectDescriptorSet* pDescSet = aDescSets[i];

      //if (pDescSet->set == static_cast<uint32_t>(ResourceFrequency::MATERIAL))
      {
        for (uint32_t j = 0u; j < pDescSet->binding_count; j++)
        {
          SpvReflectDescriptorBinding* pDesc = pDescSet->bindings[j];

          if (pDesc->descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
          {
            if (uCurrIdx == _uIdx)
            {
              if(pDesc->block.member_count > _uMemberIdx)
              {
                return pDesc->block.members[_uMemberIdx].name;
              }
              break;
            }

            uCurrIdx++;
          }
        }
      }
    }

    THROW_GENERIC_EXCEPTION("[API] Constant buffer member not found by index");

    return "";
  }

  size_t GetConstantBufferMemberSize(const RenderState* _pAPIRenderState, PipelineStage _eStage, uint32_t _uIdx, uint32_t _uMemberIdx)
  {

    uint32_t uDescSetCount = 0u;
    SpvReflectDescriptorSet* aDescSets[4];

    GetDescSetReflection(_pAPIRenderState, _eStage, aDescSets, uDescSetCount);

    uint32_t uCurrIdx = 0u;

    for (uint32_t i = 0u; i < uDescSetCount; i++)
    {
      SpvReflectDescriptorSet* pDescSet = aDescSets[i];

      //if (pDescSet->set == static_cast<uint32_t>(ResourceFrequency::MATERIAL))
      {
        for (uint32_t j = 0u; j < pDescSet->binding_count; j++)
        {
          SpvReflectDescriptorBinding* pDesc = pDescSet->bindings[j];

          if (pDesc->descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
          {
            if (uCurrIdx == _uIdx)
            {
              if (pDesc->block.member_count > _uMemberIdx)
              {
                return pDesc->block.members[_uMemberIdx].padded_size;
              }
              break;
            }

            uCurrIdx++;
          }
        }
      }
    }

    THROW_GENERIC_EXCEPTION("[API] Constant buffer member not found by index");

    return 0u;
  }

  ConstantBufferType GetConstantBufferMemberType(const RenderState* _pAPIRenderState, PipelineStage _eStage, uint32_t _uIdx, uint32_t _uMemberIdx)
  {

    uint32_t uDescSetCount = 0u;
    SpvReflectDescriptorSet* aDescSets[4];

    GetDescSetReflection(_pAPIRenderState, _eStage, aDescSets, uDescSetCount);

    uint32_t uCurrIdx = 0u;

    for (uint32_t i = 0u; i < uDescSetCount; i++)
    {
      SpvReflectDescriptorSet* pDescSet = aDescSets[i];

      //if (pDescSet->set == static_cast<uint32_t>(ResourceFrequency::MATERIAL))
      {
        for (uint32_t j = 0u; j < pDescSet->binding_count; j++)
        {
          SpvReflectDescriptorBinding* pDesc = pDescSet->bindings[j];

          if (pDesc->descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
          {
            if (uCurrIdx == _uIdx)
            {
              if (pDesc->block.member_count > _uMemberIdx)
              {                
                SpvReflectTypeFlags uTypeFlags = pDesc->block.members[_uMemberIdx].type_description->type_flags;
                if ((uTypeFlags & SPV_REFLECT_TYPE_FLAG_FLOAT) != 0u)
                {
                  switch (pDesc->block.members[_uMemberIdx].type_description->traits.numeric.vector.component_count)
                  {
                  case 0u:
                    return ConstantBufferType::SCALAR;
                    break;
                  case 2u:
                    return ConstantBufferType::VEC2;
                    break;
                  case 3u:
                    if (pDesc->block.members[_uMemberIdx].type_description->traits.numeric.matrix.row_count == 3)
                    {
                      return ConstantBufferType::MAT3;
                    }
                    else
                    {
                      return ConstantBufferType::VEC3;
                    }
                    break;
                  case 4u:
                    if (pDesc->block.members[_uMemberIdx].type_description->traits.numeric.matrix.row_count == 4)
                    {
                      return ConstantBufferType::MAT4;
                    }
                    else
                    {
                      return ConstantBufferType::VEC4;
                    }
                    break;
                  default:
                    break;
                  }
                }
              }
              break;
            }

            uCurrIdx++;
          }
        }
      }
    }

    THROW_GENERIC_EXCEPTION("[API] Constant buffer member type invalid ");


    return ConstantBufferType::NONE;
  }

  uint32_t GetConstantBufferMemberArraySize(const RenderState* _pAPIRenderState, PipelineStage _eStage, uint32_t _uIdx, uint32_t _uMemberIdx)
  {
    uint32_t uDescSetCount = 0u;
    SpvReflectDescriptorSet* aDescSets[4];

    GetDescSetReflection(_pAPIRenderState, _eStage, aDescSets, uDescSetCount);

    uint32_t uCurrIdx = 0u;

    for (uint32_t i = 0u; i < uDescSetCount; i++)
    {
      SpvReflectDescriptorSet* pDescSet = aDescSets[i];

      //if (pDescSet->set == static_cast<uint32_t>(ResourceFrequency::MATERIAL))
      {
        for (uint32_t j = 0u; j < pDescSet->binding_count; j++)
        {
          SpvReflectDescriptorBinding* pDesc = pDescSet->bindings[j];

          if (pDesc->descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
          {
            if (uCurrIdx == _uIdx)
            {
              if (pDesc->block.member_count > _uMemberIdx)
              {
                SpvReflectTypeFlags uTypeFlags = pDesc->block.members[_uMemberIdx].type_description->type_flags;
                if ((uTypeFlags & SPV_REFLECT_TYPE_FLAG_FLOAT) != 0u)
                {                  
                  uint32_t uSize = pDesc->block.members[_uMemberIdx].type_description->traits.array.dims[0];
                  if (uSize == 0u)
                  {
                    uSize = 1;
                  }
                  return uSize;
                }
              }
              break;
            }

            uCurrIdx++;
          }
        }
      }
    }

    THROW_GENERIC_EXCEPTION("[API] Constant buffer member array size invalid ");

    return 0u;
  }

  // Render substate

  ResourceSet* CreateResourceSet(RenderState* _pRenderState, uint32_t _uSetId)
  {    
    assert(_uSetId < MAX_DESCRIPTOR_SET_LAYOUTS);

    ResourceSet* pSubState = s_oResourceSetPool.PullElement();

    pSubState->m_pRenderState = _pRenderState;
    pSubState->m_uSetId = _uSetId;

    uint32_t uNumImages = s_oGlobalData.m_oWindow.m_uSwapchainImageCount;

    std::vector<VkDescriptorSetLayout> lstLayouts(uNumImages);

    if(_pRenderState->m_aDescriptorSetLayouts[_uSetId] == VK_NULL_HANDLE)
    {
      THROW_GENERIC_EXCEPTION("[API] CreateAPIResourceSet: Render state does not have a descriptor set layout for the specified set id");
    }

    for (int i = 0; i < uNumImages; i++)
    {
      lstLayouts[i] = _pRenderState->m_aDescriptorSetLayouts[_uSetId];
    }

    VkDescriptorSetAllocateInfo oDescSetAllocInfo{};
    oDescSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    oDescSetAllocInfo.descriptorPool = s_oGlobalData.m_oWindow.m_hDescPool;
    oDescSetAllocInfo.descriptorSetCount = uNumImages;
    oDescSetAllocInfo.pSetLayouts = lstLayouts.data();

    pSubState->m_pDescSets = s_oVkDescriptorSetPool.PullElements(uNumImages);

    VK_CHECK(vkAllocateDescriptorSets(s_oGlobalData.m_oWindow.m_hDevice, &oDescSetAllocInfo, pSubState->m_pDescSets))    

    return pSubState;
  }

  void BeginResourceSetSetup(ResourceSet* _pSubState)
  {
    /*if (s_oGlobalData.m_pUsingSubState != nullptr)
    {
      THROW_GENERIC_EXCEPTION("[API] EndSubStateSetup was not called before a new BeginSubStateSetup call")
    }*/
    s_oGlobalData.m_oDescSetUpdater.SetDescriptorSets(_pSubState->m_pDescSets);    
  }

  void ResourceSetAddConstantBuffer(ResourceSet* _pSubState, ConstantBuffer* _pCBuffer, size_t _uSize, const ResourceBindInfo& _oBindInfo)
  {           
    uint32_t uBinding = _pSubState->m_pRenderState->m_aLayoutBuilders[_pSubState->m_uSetId].FindBinding(_oBindInfo.m_sName, GetVkShaderFlags(_oBindInfo.m_uStageFlags), VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    AddBufferToDescSetUpdater(_pCBuffer, _uSize, uBinding, _oBindInfo.m_uStageFlags);
  }

  void ResourceSetAddTexture(ResourceSet* _pSubState, Texture* _pTexture, const ResourceBindInfo& _oBindInfo)
  {     
    uint32_t uBinding = _pSubState->m_pRenderState->m_aLayoutBuilders[_pSubState->m_uSetId].FindBinding(_oBindInfo.m_sName, GetVkShaderFlags(_oBindInfo.m_uStageFlags), VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    AddImageToDescSetUpdater(_pTexture, uBinding, _oBindInfo.m_uStageFlags);
  }

  void EndResourceSetSetup()
  {        
    s_oGlobalData.m_oDescSetUpdater.Update();
    s_oGlobalData.m_oDescSetUpdater.Clear();
  }

  void BindResourceSet(RenderState* _pRenderState, ResourceSet* _pResourceSet)
  {        
    const uint32_t uFrameIdx = s_oGlobalData.m_oWindow.m_uCurrFrameIdx;
    vkCmdBindDescriptorSets(s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, _pRenderState->m_hPipelineLayout, _pResourceSet->m_uSetId, 1, &_pResourceSet->m_pDescSets[uFrameIdx], 0, NULL);
  }

  void DestroyResourceSet(ResourceSet* _pAPIResourceSet)
  {
    const uint32_t uFrameIdx = s_oGlobalData.m_oWindow.m_uCurrFrameIdx;

    VK_CHECK(vkWaitForFences(s_oGlobalData.m_oWindow.m_hDevice, 1, &s_oGlobalData.m_oWindow.m_pInFlightFences[uFrameIdx], VK_TRUE, UINT64_MAX))

    uint32_t uNumImages = s_oGlobalData.m_oWindow.m_uSwapchainImageCount;
    vkFreeDescriptorSets(s_oGlobalData.m_oWindow.m_hDevice, s_oGlobalData.m_oWindow.m_hDescPool, uNumImages, _pAPIResourceSet->m_pDescSets);

    s_oVkDescriptorSetPool.ReturnElements(_pAPIResourceSet->m_pDescSets);

    s_oResourceSetPool.ReturnElement(_pAPIResourceSet);
  }

  // Drawing

  void WaitForLastImage(const Window* _pWindow)
  {
    VK_CHECK(vkWaitForFences(s_oGlobalData.m_oWindow.m_hDevice, 1u, &s_oGlobalData.m_oWindow.m_pInFlightFences[s_oGlobalData.m_oWindow.m_uCurrFrameIdx], VK_TRUE, UINT64_MAX))
    s_oGlobalData.m_bRenderBegan = false;
  }

  int BeginDraw()
  {

    const uint32_t uFrameIdx = s_oGlobalData.m_oWindow.m_uCurrFrameIdx;

    VK_CHECK(vkWaitForFences(s_oGlobalData.m_oWindow.m_hDevice, 1, &s_oGlobalData.m_oWindow.m_pInFlightFences[uFrameIdx], VK_TRUE, UINT64_MAX))

    VkResult hResult = VK_SUCCESS; 
    
    if (!s_oGlobalData.m_oWindow.m_bResized)
    {
      hResult = vkAcquireNextImageKHR(s_oGlobalData.m_oWindow.m_hDevice, s_oGlobalData.m_oWindow.m_hSwapchain, UINT64_MAX, s_oGlobalData.m_oWindow.m_pImageAvailableSemaphores[uFrameIdx], VK_NULL_HANDLE, &s_oGlobalData.m_oWindow.m_uCurrSwapchainImageIdx);
    }

    if (hResult == VK_ERROR_OUT_OF_DATE_KHR || hResult == VK_SUBOPTIMAL_KHR || s_oGlobalData.m_oWindow.m_bResized)
    {
      s_oGlobalData.m_oWindow.m_bResized = false;     
      s_oGlobalData.m_bRenderBegan = false;
      ImGui::EndFrame();
      RecreateSwapchain();      
      ImGui_ImplVulkan_Shutdown();
      ImGui_ImplWin32_Shutdown();
      SetupImGui();
      return 1;
    }
    else if (hResult != VK_SUCCESS)
    {
      VK_CHECK(hResult)
    }

    VK_CHECK(vkResetFences(s_oGlobalData.m_oWindow.m_hDevice, 1, &s_oGlobalData.m_oWindow.m_pInFlightFences[uFrameIdx]))

    VK_CHECK(vkResetCommandBuffer(s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx], 0))

    VkCommandBufferBeginInfo oCommandBufferBeginInfo {};
    oCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    oCommandBufferBeginInfo.flags = 0u;
    oCommandBufferBeginInfo.pInheritanceInfo = NULL;

    VK_CHECK(vkBeginCommandBuffer(s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx], &oCommandBufferBeginInfo))  

    api::ImGuiNewFrame();

    s_oGlobalData.m_bRenderBegan = true;


    return 0;
  }

  void BeginDrawOffline()
  {

    const uint32_t uFrameIdx = s_oGlobalData.m_oWindow.m_uCurrFrameIdx;

    VK_CHECK(vkWaitForFences(s_oGlobalData.m_oWindow.m_hDevice, 1, &s_oGlobalData.m_oWindow.m_pInFlightFences[uFrameIdx], VK_TRUE, UINT64_MAX))

    VK_CHECK(vkResetFences(s_oGlobalData.m_oWindow.m_hDevice, 1, &s_oGlobalData.m_oWindow.m_pInFlightFences[uFrameIdx]))

    VK_CHECK(vkResetCommandBuffer(s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx], 0))

    VkCommandBufferBeginInfo oCommandBufferBeginInfo {};
    oCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    oCommandBufferBeginInfo.flags = 0u;
    oCommandBufferBeginInfo.pInheritanceInfo = NULL;

    VK_CHECK(vkBeginCommandBuffer(s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx], &oCommandBufferBeginInfo))
  }
  
  void DrawMesh(RenderState* _pRenderState, Mesh* _pMesh, uint32_t _uIndexCount, const void* _pConstantData, uint32_t _uConstantSize)
  {

    VkCommandBuffer& hCmdBuffer = s_oGlobalData.m_oWindow.m_pCmdBuffers[s_oGlobalData.m_oWindow.m_uCurrFrameIdx];

    VkBuffer vertexBuffers[] = { _pMesh->m_hVertexBuffer };
    VkDeviceSize offsets[] = { 0u };
    vkCmdBindVertexBuffers(hCmdBuffer, 0u, 1u, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(hCmdBuffer, _pMesh->m_hIndexBuffer, 0u, VK_INDEX_TYPE_UINT16);

    vkCmdPushConstants(hCmdBuffer, _pRenderState->m_hPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, _uConstantSize, _pConstantData);

    vkCmdDrawIndexed(hCmdBuffer, _uIndexCount, 1u, 0u, 0u, 0u);

  }

  void EndDraw()
  {    

    const uint32_t uFrameIdx = s_oGlobalData.m_oWindow.m_uCurrFrameIdx;    

    VK_CHECK(vkEndCommandBuffer(s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx]))

    VkSemaphore aWaitSemaphores[] = { s_oGlobalData.m_oWindow.m_pImageAvailableSemaphores[uFrameIdx] };
    VkPipelineStageFlags aWaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore aSignalSemaphores[] = { s_oGlobalData.m_oWindow.m_pRenderFinishedSemaphores[uFrameIdx] };

    VkSubmitInfo oSubmitInfo{};
    oSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    oSubmitInfo.waitSemaphoreCount = s_oGlobalData.m_bRenderBegan ? 1 : 0;
    oSubmitInfo.pWaitSemaphores = aWaitSemaphores;
    oSubmitInfo.pWaitDstStageMask = aWaitStages;
    oSubmitInfo.commandBufferCount = 1u;
    oSubmitInfo.pCommandBuffers = &s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx];
    oSubmitInfo.signalSemaphoreCount = s_oGlobalData.m_bRenderBegan ? 1 : 0;
    oSubmitInfo.pSignalSemaphores = aSignalSemaphores;

    VK_CHECK(vkQueueSubmit(s_oGlobalData.m_oWindow.m_hRenderQueue, 1, &oSubmitInfo, s_oGlobalData.m_oWindow.m_pInFlightFences[uFrameIdx]))   
  }

  void Present()
  {

    const uint32_t uFrameIdx = s_oGlobalData.m_oWindow.m_uCurrFrameIdx;

    VkSemaphore aSignalSemaphores[] = { s_oGlobalData.m_oWindow.m_pRenderFinishedSemaphores[uFrameIdx] };

    VkSwapchainKHR swapChains[] = { s_oGlobalData.m_oWindow.m_hSwapchain };

    VkPresentInfoKHR oPresentInfo{};
    oPresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    oPresentInfo.waitSemaphoreCount = 1;
    oPresentInfo.pWaitSemaphores = aSignalSemaphores;
    oPresentInfo.swapchainCount = 1;
    oPresentInfo.pSwapchains = swapChains;
    oPresentInfo.pImageIndices = &s_oGlobalData.m_oWindow.m_uCurrSwapchainImageIdx;
    oPresentInfo.pResults = NULL;

    VK_CHECK(vkQueuePresentKHR(s_oGlobalData.m_oWindow.m_hPresentQueue, &oPresentInfo))

    //s_oGlobalData.m_oWindow.m_uCurrSwapchainImageIdx = (s_oGlobalData.m_oWindow.m_uCurrSwapchainImageIdx + 1u) % s_oGlobalData.m_oWindow.m_uSwapchainImageCount;

    s_oGlobalData.m_oWindow.m_uCurrFrameIdx = (s_oGlobalData.m_oWindow.m_uCurrFrameIdx + 1u) % s_oGlobalData.m_oWindow.m_uSwapchainImageCount;
  }

  // Misc

  void ImGuiNewFrame()
  {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
  }

}