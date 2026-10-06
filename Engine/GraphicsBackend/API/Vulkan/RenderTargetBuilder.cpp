#include "API/Vulkan/RenderTargetBuilder.h"
#include "API/Vulkan/RenderTarget.h"
#include "API/Vulkan/APIInternal.h"
#include "API/Vulkan/VulkanPools.h"
#include "API/Vulkan/VulkanMacros.h"
#include "API/Vulkan/Window.h"

namespace api
{

  void RenderTargetBuilder::Build()
  {

    bool bMultiview = false;// (m_lstColorTextures.size() > 0u) && (m_lstColorTextures[0]->m_uLayers > 1u);

    bool bHasDepthStencil = m_pDepthStencilTexture != nullptr;
    CreateRenderPass(m_lstColorTextures.size(), m_eColorFormat, bHasDepthStencil, m_eDepthStencilFormat, m_uMsaaSamples, m_pRenderTarget->m_hRenderPass, true, bMultiview);

    uint32_t uAttachmentCount = m_lstColorTextures.size() + m_lstColorResolveTextures.size() + (m_pDepthStencilTexture ? 1u : 0u);

    m_pRenderTarget->m_pImageViews = s_oVkImageViewPool.PullElements(uAttachmentCount);

    uint32_t uAttachmentIdx = 0u;

    for (uint32_t i = 0; i < m_lstColorTextures.size(); i++)
    {
      Texture* pTexture = m_lstColorTextures[i];

      CreateImageView(
        pTexture->m_hImage,
        pTexture->m_eFormat,
        m_lstColorMipLevels[i], 1u,
        0u, pTexture->m_uLayers,
        pTexture->m_uLayers > 1u ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
        VK_IMAGE_ASPECT_COLOR_BIT,
        m_pRenderTarget->m_pImageViews[uAttachmentIdx++]);

      m_pRenderTarget->m_aColorTextures[i] = pTexture;
      m_pRenderTarget->m_aColorTexturesMipLevels[i] = m_lstColorMipLevels[i];
    }

    for (uint32_t i = 0; i < m_lstColorResolveTextures.size(); i++)
    {
      Texture* pTexture = m_lstColorResolveTextures[i];
      CreateImageView(
        pTexture->m_hImage,
        pTexture->m_eFormat,
        0u, 1u,
        0u, pTexture->m_uLayers,
        pTexture->m_uLayers > 1u ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
        VK_IMAGE_ASPECT_COLOR_BIT,
        m_pRenderTarget->m_pImageViews[uAttachmentIdx++]);

      m_pRenderTarget->m_aResolveTextures[i] = pTexture;
    }    

    if(m_pDepthStencilTexture)
    {
      CreateImageView(
        m_pDepthStencilTexture->m_hImage,
        m_pDepthStencilTexture->m_eFormat,
        m_uDepthMipLevel, 1u,
        0u, m_pDepthStencilTexture->m_uLayers,
        m_pDepthStencilTexture->m_uLayers > 1u ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        m_pRenderTarget->m_pImageViews[uAttachmentIdx++]);

      m_pRenderTarget->m_pDepthTexture = m_pDepthStencilTexture;
      m_pRenderTarget->m_uDepthTextureMipLevel = m_uDepthMipLevel;
    }    

    VkFramebufferCreateInfo oFramebufferInfo = {};
    oFramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    oFramebufferInfo.attachmentCount = uAttachmentCount;
    oFramebufferInfo.pAttachments = &m_pRenderTarget->m_pImageViews[0];
    oFramebufferInfo.renderPass = m_pRenderTarget->m_hRenderPass;
    oFramebufferInfo.width = m_uWidth;
    oFramebufferInfo.height = m_uHeight;
    oFramebufferInfo.layers = (m_lstColorTextures.size() > 0u) ? m_lstColorTextures[0]->m_uLayers : 1u;

    VK_CHECK(vkCreateFramebuffer(s_oGlobalData.m_oWindow.m_hDevice, &oFramebufferInfo, NULL, &m_pRenderTarget->m_hFrameBuffer))

    m_pRenderTarget->m_uWidth = m_uWidth;
    m_pRenderTarget->m_uHeight = m_uHeight;
    m_pRenderTarget->m_uMsaaSamples = m_uMsaaSamples;
    m_pRenderTarget->m_uAttachmentCount = m_lstColorTextures.size();
  }

}