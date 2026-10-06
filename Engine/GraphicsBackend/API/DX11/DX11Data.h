#pragma once

#include "RenderTargetBuilder.h"
#include "API/DX11/Window.h"

#define MAX_BOUND_TEXTURES_PER_STAGE 16u

namespace api
{  
  struct Texture;

  struct DX11Data
  {

    Window m_oWindow;

    RenderTargetBuilder m_oRenderTargetBuilder;

    Texture* m_aPSBoundTextures[MAX_BOUND_TEXTURES_PER_STAGE];
    Texture* m_aGSBoundTextures[MAX_BOUND_TEXTURES_PER_STAGE];
    Texture* m_aVSBoundTextures[MAX_BOUND_TEXTURES_PER_STAGE];

    uint32_t m_aPSBoundTextureSlots[MAX_BOUND_TEXTURES_PER_STAGE];
    uint32_t m_aGSBoundTextureSlots[MAX_BOUND_TEXTURES_PER_STAGE];
    uint32_t m_aVSBoundTextureSlots[MAX_BOUND_TEXTURES_PER_STAGE];

    uint32_t m_uPSBoundTextureCount = 0u;
    uint32_t m_uGSBoundTextureCount = 0u;
    uint32_t m_uVSBoundTextureCount = 0u;

    uint32_t m_uMaxMsaaSamples = 1u;    

  };
}