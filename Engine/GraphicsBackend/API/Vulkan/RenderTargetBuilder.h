#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace api
{

  struct RenderTarget;  

  struct Texture;  

  struct Window;

  class RenderTargetBuilder
  {
  public:

    void SetRenderTarget(RenderTarget* _pRenderTarget)
    {
      m_pRenderTarget = _pRenderTarget;
    }

    void AddColorTexture(Texture* _pTexture, uint32_t _uMipLevel)
    {
      m_lstColorTextures.push_back(_pTexture);
      m_lstColorMipLevels.push_back(_uMipLevel);
    }

    void SetDepthTexture(Texture* _pTexture, uint32_t _uMipLevel)
    {
      m_pDepthStencilTexture = _pTexture;
      m_uDepthMipLevel = _uMipLevel;
    }

    void AddResolveColorTexture(Texture* _pTexture)
    {
      m_lstColorResolveTextures.push_back(_pTexture);
    }

    void Build();

    void Clear()
    {
      m_lstColorTextures.clear();
      m_lstColorResolveTextures.clear();
      m_lstColorMipLevels.clear();
      m_pDepthStencilTexture = nullptr;      
    }

  public:

    VkFormat m_eColorFormat;

    VkFormat m_eDepthStencilFormat;

    uint32_t m_uMsaaSamples;

    uint32_t m_uWidth, m_uHeight;

  private:    

    RenderTarget* m_pRenderTarget;
    std::vector<Texture*> m_lstColorTextures;
    std::vector<Texture*> m_lstColorResolveTextures;
    std::vector<uint32_t> m_lstColorMipLevels;
    uint32_t m_uDepthMipLevel;
    Texture* m_pDepthStencilTexture = nullptr;
  };

}