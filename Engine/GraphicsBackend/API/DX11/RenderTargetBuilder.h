#pragma once

#include <vector>

#include <d3d11.h>

#include "DX11Macros.h"
#include "RenderTarget.h"
#include "Texture.h"
#include "Window.h"

namespace api
{
  class RenderTargetBuilder
  {
  public:

    void AddColorTexture(Texture* _pTexture, uint32_t _uMipLevel) { m_lstColorTextures.push_back(_pTexture); m_lstColorMipLevels.push_back(_uMipLevel); }

    void SetDepthTexture(Texture* _pTexture, uint32_t _uMipLevel) { m_pDepthStencilTexture = _pTexture; m_uDepthMipLevel = _uMipLevel; }

    void AddResolveColorTexture(Texture* _pTexture){ m_lstColorResolveTextures.push_back(_pTexture); }           

    void Build();

    void Clear();

  public:

    DXGI_FORMAT m_eColorFormat;
    DXGI_FORMAT m_eDepthStencilFormat;

    uint32_t m_uMsaaSamples;

    uint32_t m_uWidth, m_uHeight;

    bool m_bIsCubemap = false;

    RenderTarget* m_pRenderTarget = nullptr;

  private:

    std::vector<Texture*> m_lstColorTextures;
    std::vector<Texture*> m_lstColorResolveTextures;
    Texture* m_pDepthStencilTexture = nullptr;      

    std::vector<uint32_t> m_lstColorMipLevels;
    uint32_t m_uDepthMipLevel;
  };  
}