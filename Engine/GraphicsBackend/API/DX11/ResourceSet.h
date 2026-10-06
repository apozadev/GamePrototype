#pragma once

#define MAX_TEXTURE_BINDINGS_PER_SET 16
#define MAX_CBUFFER_BINDINGS_PER_SET 16

namespace api
{

struct RenderState;
struct Texture;
struct ConstantBuffer;

struct ResourceSetTextureBinding
{
  uint32_t m_uBinding;
  PipelineStageFlags m_uStageFlags;
  Texture* m_pTexture;
};

struct ResourceSetCBufferBinding
{
  uint32_t m_uBinding;
  PipelineStageFlags m_uStageFlags;
  ConstantBuffer* m_pConstantBuffer;
};

struct ResourceSet
{
  ResourceSetTextureBinding m_lstTextures[MAX_TEXTURE_BINDINGS_PER_SET];
  ResourceSetCBufferBinding m_lstCBuffers[MAX_CBUFFER_BINDINGS_PER_SET];

  uint32_t m_uTextureBindingCount = 0;
  uint32_t m_uCBufferBindingCount = 0;

  RenderState* m_pRenderState = nullptr;
};

}
