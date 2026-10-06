#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <inttypes.h>

#include "ImageFormat.h"
#include "BlendEnums.h"
#include "CullMode.h"
#include "DepthCompareOp.h"
#include "SamplerConfig.h"
#include "PipelineStage.h"
#include "API/DX11/DX11Data.h"

namespace api
{

  struct RenderState;

  extern DX11Data s_oGlobalData;

  void PollWindowSize();

  DXGI_FORMAT GetDXGIFormat(ImageFormat _eFormat);

  DXGI_FORMAT GetDXGISrvFormat(ImageFormat _eFormat);

  uint32_t GetTextureUsage(uint32_t _uUsage);

  D3D11_BLEND_OP GetD3D11BlendOp(BlendOp _eBlendOp);

  D3D11_BLEND GetD3D11Blend(BlendFactor _eBlendFactor);

  ImageFormat GetImageFormat(DXGI_FORMAT _eFormat);  

  D3D11_TEXTURE_ADDRESS_MODE GetD3D11AdressMode(TextureAddressMode _eAddressMode);

  D3D11_FILTER GetD3D11Filter(TextureFilterMode _eMipmapFilter, TextureFilterMode _eMinFilter, TextureFilterMode _eMagFilter);

  D3D11_CULL_MODE GetD3D11CullMode(CullMode _eCullMode);

  D3D11_COMPARISON_FUNC GetD3D11DepthCompareFunc(DepthCompareOp _eCompareOp);

  void CreateDeviceAndSwapChain();

  void CreateSwapchainViewsAndViewport();

  uint32_t QueryMaxSupportedMsaaSamples();

  void CreateWindowRenderTarget();

  void ResizeSwapchain();  

  bool GetBufferByIndex(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx, ID3D11ShaderReflectionConstantBuffer** ppCBuffer_, D3D11_SHADER_BUFFER_DESC& rDesc_);

  uint32_t GetBindFromReflection(ID3D11ShaderReflection* _pReflection, const std::string& _sName, _D3D_SHADER_INPUT_TYPE _eInputType);

}
