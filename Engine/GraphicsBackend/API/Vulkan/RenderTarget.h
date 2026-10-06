#pragma once

#include <vulkan/vulkan.h>

namespace api
{
  struct Texture;

  struct RenderTarget
  {
    VkRenderPass m_hRenderPass;
    VkFramebuffer m_hFrameBuffer;

    uint32_t m_uAttachmentCount;

    VkImageView* m_pImageViews;

    uint32_t m_uWidth, m_uHeight;

    uint32_t m_uMsaaSamples;

    Texture* m_aColorTextures[4];
    Texture* m_aResolveTextures[4];
    Texture* m_pDepthTexture;

    uint32_t m_aColorTexturesMipLevels[4];
    uint32_t m_uDepthTextureMipLevel;
  };

}