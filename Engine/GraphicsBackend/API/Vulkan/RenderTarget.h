#pragma once

#include <vulkan/vulkan.h>

namespace api
{
  struct Texture;

  constexpr uint32_t MAX_RENDER_TARGET_COLOR_ATTACHMENTS = 4u;

  // Color + resolve + depth
  constexpr uint32_t MAX_RENDER_TARGET_ATTACHMENTS = 2u * MAX_RENDER_TARGET_COLOR_ATTACHMENTS + 1u;

  struct RenderTarget
  {
    VkRenderPass m_hRenderPass;
    VkFramebuffer m_hFrameBuffer;

    uint32_t m_uAttachmentCount;

    VkImageView m_aImageViews[MAX_RENDER_TARGET_ATTACHMENTS];

    uint32_t m_uWidth, m_uHeight;

    uint32_t m_uMsaaSamples;

    Texture* m_aColorTextures[MAX_RENDER_TARGET_COLOR_ATTACHMENTS];
    Texture* m_aResolveTextures[MAX_RENDER_TARGET_COLOR_ATTACHMENTS];
    Texture* m_pDepthTexture;

    uint32_t m_aColorTexturesMipLevels[MAX_RENDER_TARGET_COLOR_ATTACHMENTS];
    uint32_t m_uDepthTextureMipLevel;
  };

}