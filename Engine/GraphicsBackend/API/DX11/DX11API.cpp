#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <dxgi.h>
#include <cmath>

#include "API/GraphicsAPI.h"

#include "ShaderCode.h"
#include "BlendEnums.h"
#include "Asserts.h"
#include "Log.h"
#include "PipelineStage.h"
#include "SamplerConfig.h"
#include "ImageFormat.h"
#include "TextureUsage.h"
#include "ResourceBindInfo.h"
#include "TextureSubResource.h"
#include "RenderStateInfo.h"
#include "ResourceSet.h"
#include "Vertex.h"
#include "ConstantBufferTypes.h"
#include "API/DX11/DX11Macros.h"
#include "API/DX11/DX11Data.h"
#include "API/DX11/Window.h"
#include "API/DX11/RenderState.h"
#include "API/DX11/ConstantBuffer.h"
#include "API/DX11/Mesh.h"
#include "API/DX11/Texture.h"
#include "API/DX11/RenderTarget.h"
#include "API/DX11/DX11BindSlotOffset.h"
#include "API/DX11/APIInternal.h"
#include "APIAllocation.h"

namespace wrl = Microsoft::WRL;

#define INVALID_BINDING_SLOT 0xFFFFFFFFu

namespace api
{
  // General

  void Initialize()
  {
    s_oGlobalData.m_uMaxMsaaSamples = 0u;
  }

  void ShutDown()
  {
  }

  uint32_t GetMaxMsaaSamples()
  {
    return s_oGlobalData.m_uMaxMsaaSamples;
  }

  // Window


  void CreateAPIWindow(LinearAllocator* _pAllocator, HWND _hwnd, uint32_t _uMsaaSamples)
  {      

    s_oGlobalData.m_oWindow.m_hWnd = _hwnd;

    s_oGlobalData.m_oWindow.m_uNumSwapchainImages = 2u;

    s_oGlobalData.m_oWindow.m_eSwapchainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;            
      
    PollWindowSize();
    CreateDeviceAndSwapChain();      
      
    if (s_oGlobalData.m_uMaxMsaaSamples < 1u)
    {
      s_oGlobalData.m_uMaxMsaaSamples = QueryMaxSupportedMsaaSamples();
    }
      
    if (_uMsaaSamples > s_oGlobalData.m_uMaxMsaaSamples)
    {
      s_oGlobalData.m_oWindow.m_uMsaaSamples = s_oGlobalData.m_uMaxMsaaSamples;
    }
    else
    {
      s_oGlobalData.m_oWindow.m_uMsaaSamples = _uMsaaSamples;
    }

    CreateWindowRenderTarget();

    // Default to triangle list just in case
    s_oGlobalData.m_oWindow.m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    SamplerConfig oSamplerCfg = {};
    oSamplerCfg.m_eAddressMode = TextureAddressMode::CLAMP;
    oSamplerCfg.m_eMagFilterMode = TextureFilterMode::POINT;
    oSamplerCfg.m_eMinFilterMode = TextureFilterMode::POINT;
    oSamplerCfg.m_eMipmapFilterMode = TextureFilterMode::POINT;

    uint8_t aDummyBuffer[4] = { 255u, 20u, 0u, 255u };
    void* pDummyBuffer = &(aDummyBuffer[0]);
    s_oGlobalData.m_oWindow.m_pDummyTexture = CreateTexture(_pAllocator, &pDummyBuffer, 1u, 1u, ImageFormat::R8G8B8A8, 1u, 1u, TextureUsage::SHADER_RESOURCE, oSamplerCfg, false);
    FATAL_ASSERT_MSG(s_oGlobalData.m_oWindow.m_pDummyTexture, "[DX11] Could not create the dummy texture (see log)");

#ifndef NDEBUG
    // Optional: only there when the device has the debug layer
    Microsoft::WRL::ComPtr<ID3D11Debug> d3dDebug;
    Microsoft::WRL::ComPtr<ID3D11InfoQueue> d3dInfoQueue;
    if (DX11_OK(s_oGlobalData.m_oWindow.m_pDevice.As(&d3dDebug)) && DX11_OK(d3dDebug.As(&d3dInfoQueue)))
    {
    D3D11_MESSAGE_ID hide[] =
    {
      D3D11_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET,
    };
    D3D11_INFO_QUEUE_FILTER filter;
    memset(&filter, 0, sizeof(filter));
    filter.DenyList.NumIDs = _countof(hide);
    filter.DenyList.pIDList = hide;
    d3dInfoQueue->AddStorageFilterEntries(&filter);
    }
#endif
  }

  bool IsWindowCreated()
  {
    return s_oGlobalData.m_oWindow.m_hWnd != NULL;
  }

  void OnWindowResize()
  {

    s_oGlobalData.m_oWindow.m_bResize = true;

  }

  uint32_t GetWindowWidth()
  {
    return s_oGlobalData.m_oWindow.m_uWidth;
  }

  uint32_t GetWindowHeight()
  {
    return s_oGlobalData.m_oWindow.m_uHeight;
  }

  uint32_t GetWindowMSAASamples()
  {
    return s_oGlobalData.m_oWindow.m_uMsaaSamples;
  }

  void ClearDefaultRenderTarget()
  {
    float aClearColor[] = { 0.f,0.f,0.f,1.f };
    unsigned int uClearFlags = D3D11_CLEAR_DEPTH;

    s_oGlobalData.m_oWindow.m_pContext->ClearRenderTargetView(s_oGlobalData.m_oWindow.m_pRtv.Get(), aClearColor);

    s_oGlobalData.m_oWindow.m_pContext->ClearDepthStencilView(s_oGlobalData.m_oWindow.m_pDsv.Get(), uClearFlags, 1.0f, 0u);
  }

  void BindDefaultRenderTarget()
  {      
    s_oGlobalData.m_oWindow.m_pContext->OMSetRenderTargets(1, s_oGlobalData.m_oWindow.m_pRtv.GetAddressOf(), s_oGlobalData.m_oWindow.m_pDsv.Get());

    D3D11_VIEWPORT oViewport;
    oViewport.Width = static_cast<FLOAT>(s_oGlobalData.m_oWindow.m_uWidth);
    oViewport.Height = static_cast<FLOAT>(s_oGlobalData.m_oWindow.m_uHeight);
    oViewport.MinDepth = 0.0f;
    oViewport.MaxDepth = 1.0f;
    oViewport.TopLeftX = 0.0f;
    oViewport.TopLeftY = 0.0f;

    s_oGlobalData.m_oWindow.m_pContext->RSSetViewports(1, &oViewport);
  }

  void UnbindDefaultRenderTarget()
  {
    ID3D11RenderTargetView* pNullView =  nullptr;
    s_oGlobalData.m_oWindow.m_pContext->OMSetRenderTargets(1, &pNullView, nullptr);
  }

  void DestroyWindow()
  {
    DestroyTexture(s_oGlobalData.m_oWindow.m_pDummyTexture);

    s_oGlobalData.m_oWindow.m_pRtv = nullptr;
    s_oGlobalData.m_oWindow.m_pDsv = nullptr;
    s_oGlobalData.m_oWindow.m_pColorTexture = nullptr;
    s_oGlobalData.m_oWindow.m_pDSTexture = nullptr;
    s_oGlobalData.m_oWindow.m_pSwapchain = nullptr;
    s_oGlobalData.m_oWindow.m_pContext = nullptr;

#ifndef NDEBUG
    Microsoft::WRL::ComPtr<ID3D11Debug> pDebug;
    s_oGlobalData.m_oWindow.m_pDevice.As(&pDebug);
    pDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
#endif

    s_oGlobalData.m_oWindow.m_pDevice = nullptr;
  }

  // Mesh

  Mesh* CreateMesh(LinearAllocator* _pAllocator, const void* _pVertexData, size_t _uVertexDataSize, const void* _pIndexData, size_t _uIndexDataSize, size_t /*_uMeshConstantSize*/)
  {

    Mesh* pMesh = NewObject<Mesh>(_pAllocator);
    CreationGuard<Mesh> oGuard(pMesh);

    // Vertex buffer

    D3D11_BUFFER_DESC oVBDesc = {};
    oVBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    oVBDesc.Usage = D3D11_USAGE_IMMUTABLE;
    oVBDesc.CPUAccessFlags = 0u;
    oVBDesc.MiscFlags = 0u;
    oVBDesc.ByteWidth = static_cast<UINT>(_uVertexDataSize);
    oVBDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA oVBData = {};
    oVBData.pSysMem = _pVertexData;

    if (!DX11_OK(s_oGlobalData.m_oWindow.m_pDevice->CreateBuffer(&oVBDesc, &oVBData, pMesh->m_pVertexBuffer.GetAddressOf())))
    {
      return nullptr;
    }

    // Index buffer

    D3D11_BUFFER_DESC oIBDesc = {};
    oIBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    oIBDesc.Usage = D3D11_USAGE_IMMUTABLE;
    oIBDesc.CPUAccessFlags = 0u;
    oIBDesc.MiscFlags = 0u;
    oIBDesc.ByteWidth = static_cast<UINT>(_uIndexDataSize);
    oIBDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA oIBData = {};
    oIBData.pSysMem = _pIndexData;
    oIBData.SysMemPitch = 0;
    oIBData.SysMemSlicePitch = 0;

    if (!DX11_OK(s_oGlobalData.m_oWindow.m_pDevice->CreateBuffer(&oIBDesc, &oIBData, pMesh->m_pIndexBuffer.GetAddressOf())))
    {
      return nullptr;
    }

    pMesh->m_uIndexCount = static_cast<uint32_t>(_uIndexDataSize / sizeof(uint16_t));

    // Model CBuffer
    /*pMesh->m_pModelCBuffer = CreateConstantBuffer(g_pWindow, _uMeshConstantSize);

    ResourceBindInfo oBindInfo = {};
    oBindInfo.m_uStageFlags = STAGE_VERTEX;
    oBindInfo.m_sName = "ModelBuffer";

    ResourceSetAddConstantBuffer(g_pWindow, pMesh->m_pModelCBuffer, _uMeshConstantSize, oBindInfo);*/

    return oGuard.Release();
  }

  void DestroyMesh(Mesh* _pMesh)
  {
    //DestroyConstantBuffer(g_pWindow, _pMesh->m_pModelCBuffer);
    _pMesh->~Mesh();
  }

  // Constant buffer

  ConstantBuffer* CreateConstantBuffer(LinearAllocator* _pAllocator, size_t _uSize)
  {
    ConstantBuffer* pCBuffer = NewObject<ConstantBuffer>(_pAllocator);
    CreationGuard<ConstantBuffer> oGuard(pCBuffer);

    D3D11_BUFFER_DESC oDesc = {};
    oDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    oDesc.Usage = D3D11_USAGE_DEFAULT;//D3D11_USAGE_DYNAMIC;
    oDesc.CPUAccessFlags = 0u;// D3D11_CPU_ACCESS_WRITE;
    oDesc.MiscFlags = 0u;
    oDesc.ByteWidth = static_cast<UINT>(_uSize);
    oDesc.StructureByteStride = 0;

    if (!DX11_OK(s_oGlobalData.m_oWindow.m_pDevice->CreateBuffer(&oDesc, NULL, pCBuffer->m_pCBuffer.GetAddressOf())))
    {
      return nullptr;
    }
      
    return oGuard.Release();
  }

  void UpdateConstantBuffer(ConstantBuffer* _pCbuffer, const void* _pData, size_t /*_uSize*/)
  {            
    /*D3D11_MAPPED_SUBRESOURCE oMsr;
    DX11_CHECK_FATAL(pWindow->m_pContext->Map(_pCBuffer->m_pCBuffer.Get(), 0u, D3D11_MAP_WRITE_DISCARD, 0u, &oMsr));

    memcpy(oMsr.pData, &_pData, _uSize);

    pWindow->m_pContext->Unmap(_pCBuffer->m_pCBuffer.Get(), 0u);*/

    s_oGlobalData.m_oWindow.m_pContext->UpdateSubresource(_pCbuffer->m_pCBuffer.Get(), 0u, nullptr, _pData, 0u, 0u);

  }

  void DestroyConstantBuffer(ConstantBuffer* _pCBuffer)
  {
    _pCBuffer->~ConstantBuffer();
  }

  // Texture

  Texture* CreateTexture(LinearAllocator* _pAllocator, const void* const* _ppData, uint32_t _uWidth, uint32_t _uHeight, ImageFormat _eFormat, uint32_t _uMipLevels, uint32_t _uMsaaSamples, uint32_t _uUsage, const SamplerConfig& _rSamplerConfig, bool _bIsCubemap)
  {
    Texture* pTexture = NewObject<Texture>(_pAllocator);
    CreationGuard<Texture> oGuard(pTexture);

    uint32_t uPixelSize = static_cast<uint32_t>(GetImageFormatSize(_eFormat));

    uint32_t uMemPitch = uPixelSize * _uWidth;

    DXGI_FORMAT eDXGIFormat = GetDXGIFormat(_eFormat);
    DXGI_FORMAT eDXGISrvFormat = GetDXGISrvFormat(_eFormat);

    uint32_t uMaxMipLevels = static_cast<uint32_t>(std::floor(std::log2(max(_uWidth, _uHeight)))) + 1;
    pTexture->m_uMipLevels = _uMipLevels == 0u ? uMaxMipLevels : min(_uMipLevels, uMaxMipLevels);

    uint32_t uBindFlags = GetTextureUsage(_uUsage);
    if (pTexture->m_uMipLevels > 1u)
    {
      uBindFlags |= D3D11_BIND_RENDER_TARGET;
    }      

    uint32_t uLayers = _bIsCubemap ? 6u : 1u;

    D3D11_TEXTURE2D_DESC oTexDesc = {};
    oTexDesc.ArraySize = uLayers;
    oTexDesc.BindFlags = uBindFlags;
    oTexDesc.CPUAccessFlags = 0;
    oTexDesc.Format = eDXGIFormat;
    oTexDesc.SampleDesc.Count = _uMsaaSamples;
    oTexDesc.SampleDesc.Quality = 0;
    oTexDesc.Usage = D3D11_USAGE_DEFAULT;
    oTexDesc.MipLevels = pTexture->m_uMipLevels;
    oTexDesc.Width = _uWidth;
    oTexDesc.Height = _uHeight;      

    if (pTexture->m_uMipLevels != 1)
    {
      oTexDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }
      
    if(_bIsCubemap)
    {
      oTexDesc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
    }

    if (pTexture->m_uMipLevels == 1)
    {
      if (_bIsCubemap)
      {          
        D3D11_SUBRESOURCE_DATA aData[6];
        if (_ppData != nullptr)
        {
          for (int i = 0; i < 6; i++)
          {
            aData[i].pSysMem = _ppData[i];
            aData[i].SysMemPitch = uMemPitch;
          }
        }

        if (!DX11_OK(s_oGlobalData.m_oWindow.m_pDevice->CreateTexture2D(&oTexDesc, _ppData ? &aData[0] : nullptr, pTexture->m_pTexture.GetAddressOf())))
        {
          return nullptr;
        }
      }
      else
      {
        D3D11_SUBRESOURCE_DATA oData = {};
        oData.pSysMem = _ppData ? *_ppData : nullptr;
        oData.SysMemPitch = uMemPitch;

        if (!DX11_OK(s_oGlobalData.m_oWindow.m_pDevice->CreateTexture2D(&oTexDesc, _ppData ? &oData : nullptr, pTexture->m_pTexture.GetAddressOf())))
        {
          return nullptr;
        }
      }
    }
    else
    {
      //GFX_THROW_INFO(GetDevice()->CreateTexture2D(&desc, NULL, m_texture.GetAddressOf()));
      if (!DX11_OK(s_oGlobalData.m_oWindow.m_pDevice->CreateTexture2D(&oTexDesc, nullptr, pTexture->m_pTexture.GetAddressOf())))
      {
        return nullptr;
      }
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC oSrvDesc = {};
    oSrvDesc.Format = eDXGISrvFormat;

    if (_bIsCubemap)
    {
      oSrvDesc.TextureCube.MipLevels = pTexture->m_uMipLevels;
      oSrvDesc.TextureCube.MostDetailedMip = 0u;
      oSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    }
    else
    {                
      oSrvDesc.Texture2D.MipLevels = pTexture->m_uMipLevels;
      oSrvDesc.Texture2D.MostDetailedMip = 0u;
      oSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;        
    }

    if (!DX11_OK(s_oGlobalData.m_oWindow.m_pDevice->CreateShaderResourceView(pTexture->m_pTexture.Get(), &oSrvDesc, pTexture->m_pSRV.GetAddressOf())))
    {
      return nullptr;
    }

    if (_ppData != nullptr && pTexture->m_uMipLevels != 1)
    {
      if (_bIsCubemap)
      {
        for (int i = 0; i < 6; i++)
        {
          UINT uSubResource = D3D11CalcSubresource(0, i, pTexture->m_uMipLevels);
          s_oGlobalData.m_oWindow.m_pContext->UpdateSubresource(pTexture->m_pTexture.Get(), uSubResource, nullptr, _ppData[i], uMemPitch, 0u);
        }
      }
      else
      {
        s_oGlobalData.m_oWindow.m_pContext->UpdateSubresource(pTexture->m_pTexture.Get(), 0u, nullptr, *_ppData, uMemPitch, 0u);
      }

      s_oGlobalData.m_oWindow.m_pContext->GenerateMips(pTexture->m_pSRV.Get());
    }

    pTexture->m_eFormat = eDXGIFormat;
    pTexture->m_iWidth = _uWidth;
    pTexture->m_iHeight = _uHeight;
    pTexture->m_uLayers = uLayers;

    D3D11_SAMPLER_DESC oSamplerDesc{};
    oSamplerDesc.AddressU = GetD3D11AdressMode(_rSamplerConfig.m_eAddressMode);
    oSamplerDesc.AddressV = GetD3D11AdressMode(_rSamplerConfig.m_eAddressMode);
    oSamplerDesc.AddressW = GetD3D11AdressMode(_rSamplerConfig.m_eAddressMode);
    oSamplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;//GetD3D11Filter(_rSamplerConfig.m_eMipmapFilterMode, _rSamplerConfig.m_eMinFilterMode, _rSamplerConfig.m_eMagFilterMode);
    oSamplerDesc.MaxAnisotropy = 16;
    oSamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    oSamplerDesc.MinLOD = 0;
    oSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;      

    s_oGlobalData.m_oWindow.m_pDevice->CreateSamplerState(&oSamplerDesc, pTexture->m_pSampler.GetAddressOf());

    return oGuard.Release();
  }

  void GenerateMipMaps(Texture* _pTexture)
  {
    s_oGlobalData.m_oWindow.m_pContext->GenerateMips(_pTexture->m_pSRV.Get());
  }

  void ClearTexture(Texture* /*_pTexture*/, TextureUsage /*_eUsage*/)
  {

  }

  void BlitTexture(Texture* _pSrcTexture, const TextureSubResource& _rSrcSubResource, Texture* _pDstTexture, const TextureSubResource& _rDstSubResource)
  {

    // Invalid blits are programming errors: assert and skip
    const bool bCountsMatch = _rSrcSubResource.m_uMipLevels == _rDstSubResource.m_uMipLevels && _rSrcSubResource.m_uLayers == _rDstSubResource.m_uLayers;
    const bool bMipsInRange = (_rSrcSubResource.m_uBaseMip + _rSrcSubResource.m_uMipLevels) <= _pSrcTexture->m_uMipLevels
      && (_rDstSubResource.m_uBaseMip + _rDstSubResource.m_uMipLevels) <= _pDstTexture->m_uMipLevels;
    const bool bLayersInRange = (_rSrcSubResource.m_uBaseLayer + _rSrcSubResource.m_uLayers) <= _pSrcTexture->m_uLayers
      && (_rDstSubResource.m_uBaseLayer + _rDstSubResource.m_uLayers) <= _pDstTexture->m_uLayers;
    ASSERT_MSG(bCountsMatch, "BlitTexture: subresource mip level count and layer count must match");
    ASSERT_MSG(bMipsInRange, "BlitTexture: subresource mip levels are outside of the texture mip level range");
    ASSERT_MSG(bLayersInRange, "BlitTexture: subresource array layers are outside of the texture array layer range");
    if (!bCountsMatch || !bMipsInRange || !bLayersInRange)
    {
      return;
    }

    for (uint32_t uMipLevel = 0; uMipLevel < _rSrcSubResource.m_uMipLevels; uMipLevel++)
    {
      for (uint32_t uLayer = 0u; uLayer < _rSrcSubResource.m_uLayers; uLayer++)
      {
        uint32_t uSrcSubRes = D3D11CalcSubresource(uMipLevel + _rSrcSubResource.m_uBaseMip, uLayer + _rSrcSubResource.m_uBaseLayer, _pSrcTexture->m_uMipLevels);
        uint32_t uDstSubRes = D3D11CalcSubresource(uMipLevel + _rDstSubResource.m_uBaseMip, uLayer + _rDstSubResource.m_uBaseLayer, _pDstTexture->m_uMipLevels);

        s_oGlobalData.m_oWindow.m_pContext->CopySubresourceRegion(
          _pDstTexture->m_pTexture.Get(),
          uDstSubRes,
          0u, 0u, 0u,
          _pSrcTexture->m_pTexture.Get(),
          uSrcSubRes,
          nullptr);
      }
    }
  }

  void DestroyTexture(Texture* _pTexture)
  {
    _pTexture->~Texture();
  }

  // RenderTarget

  RenderTarget* CreateRenderTarget(LinearAllocator* _pAllocator)
  {
    return NewObject<RenderTarget>(_pAllocator);
  }

  void BeginRenderTargetSetup(RenderTarget* _pRenderTarget, uint32_t _uWidth, uint32_t _uHeight, ImageFormat _eFormat, ImageFormat _eDepthStencilFormat, uint32_t _uMsaaSamples, bool _bIsCubemap)
  {
    s_oGlobalData.m_oRenderTargetBuilder.m_uWidth = _uWidth;
    s_oGlobalData.m_oRenderTargetBuilder.m_uHeight = _uHeight;
    s_oGlobalData.m_oRenderTargetBuilder.m_eColorFormat = GetDXGIFormat(_eFormat);
    s_oGlobalData.m_oRenderTargetBuilder.m_eDepthStencilFormat = GetDXGIFormat(_eDepthStencilFormat);
    s_oGlobalData.m_oRenderTargetBuilder.m_uMsaaSamples = _uMsaaSamples;
    s_oGlobalData.m_oRenderTargetBuilder.m_bIsCubemap = _bIsCubemap;
    s_oGlobalData.m_oRenderTargetBuilder.m_pRenderTarget = _pRenderTarget;
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

  bool EndRenderTargetSetup()
  {
    const bool bBuilt = s_oGlobalData.m_oRenderTargetBuilder.Build();
    s_oGlobalData.m_oRenderTargetBuilder.Clear();
    return bBuilt;
  }

  void ClearRenderTarget(RenderTarget* _pRenderTarget)
  {
    float aClearColor[] = { 0.f,0.f,0.f,1.f };
    unsigned int uClearFlags = D3D11_CLEAR_DEPTH;

    for (auto& pRtv : _pRenderTarget->m_lstRtv)
    {
      s_oGlobalData.m_oWindow.m_pContext->ClearRenderTargetView(pRtv.Get(), aClearColor);
    }

    if (_pRenderTarget->m_pDsv.Get())
    {
      s_oGlobalData.m_oWindow.m_pContext->ClearDepthStencilView(_pRenderTarget->m_pDsv.Get(), uClearFlags, 1.0f, 0u);
    }

  }

  void BindRenderTarget(RenderTarget* _pRenderTarget)
  {   
    // Check bound textures to unbind
    for (uint32_t i = 0; i < s_oGlobalData.m_uVSBoundTextureCount; i++)
    {
      for(Texture* pRTTexture : _pRenderTarget->m_lstTextures)
      {
        if (s_oGlobalData.m_aVSBoundTextures[i] == pRTTexture)
        {
          ID3D11ShaderResourceView* pNullSrv = nullptr;
          s_oGlobalData.m_oWindow.m_pContext->VSSetShaderResources(s_oGlobalData.m_aVSBoundTextureSlots[i], 1, &pNullSrv);

          s_oGlobalData.m_uVSBoundTextureCount--;          
          s_oGlobalData.m_aVSBoundTextures[i] = s_oGlobalData.m_aVSBoundTextures[s_oGlobalData.m_uVSBoundTextureCount];
          s_oGlobalData.m_aVSBoundTextureSlots[i] = s_oGlobalData.m_aVSBoundTextureSlots[s_oGlobalData.m_uVSBoundTextureCount];
          i--;
        }
      }
    }

    if (_pRenderTarget->m_lstRtv.size() > 0u)
    {
      s_oGlobalData.m_oWindow.m_pContext->OMSetRenderTargets(static_cast<UINT>(_pRenderTarget->m_lstRtv.size()), _pRenderTarget->m_lstRtv[0].GetAddressOf(), _pRenderTarget->m_pDsv.Get());
    }
    else
    {
      s_oGlobalData.m_oWindow.m_pContext->OMSetRenderTargets(0u, nullptr, _pRenderTarget->m_pDsv.Get());
    }

    D3D11_VIEWPORT oViewport;
    oViewport.Width = static_cast<FLOAT>(_pRenderTarget->m_iWidth);
    oViewport.Height = static_cast<FLOAT>(_pRenderTarget->m_iHeight);
    oViewport.MinDepth = 0.0f;
    oViewport.MaxDepth = 1.0f;
    oViewport.TopLeftX = 0.0f;
    oViewport.TopLeftY = 0.0f;

    s_oGlobalData.m_oWindow.m_pContext->RSSetViewports(1, &oViewport);
  }

  void UnbindRenderTarget(RenderTarget* _pRenderTarget)
  {

    //const std::vector<ID3D11RenderTargetView*> lstNullViews(_pRenderTarget->m_lstRtv.size(), nullptr);
    //s_oGlobalData.m_pUsingWindow->m_pContext->OMSetRenderTargets(_pRenderTarget->m_lstRtv.size(), lstNullViews.data(), nullptr);

    if (_pRenderTarget->m_lstRtv.size() > 0u)
    {
      ID3D11RenderTargetView* aNullViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
      s_oGlobalData.m_oWindow.m_pContext->OMSetRenderTargets(static_cast<UINT>(_pRenderTarget->m_lstRtv.size()), aNullViews, nullptr);
    }
    else
    {
      s_oGlobalData.m_oWindow.m_pContext->OMSetRenderTargets(0u, nullptr, nullptr);
    }
  }
    
  void DestroyRenderTarget(RenderTarget* _pRenderTarget)
  {
    _pRenderTarget->~RenderTarget();
  }

  // Render state

  RenderState* CreateRenderState(LinearAllocator* _pAllocator, const RenderStateInfo& _oInfo, const RenderTarget* /*_pRenderTarget*/, uint32_t _uMsaaSamples)
  {

    RenderState* pRenderState = NewObject<RenderState>(_pAllocator);
    CreationGuard<RenderState> oGuard(pRenderState);

    // The bytecode is only needed while creating the render state
    ScopedRollback oShaderCode(_pAllocator);

    ShaderCode oVs;
    ShaderCode oPs;
    if (!LoadShaderCode(_pAllocator, _oInfo.m_sVSFilename.data(), oVs) || !LoadShaderCode(_pAllocator, _oInfo.m_sPSFilename.data(), oPs))
    {
      return nullptr;
    }
      
    if (!DX11_OK(s_oGlobalData.m_oWindow.m_pDevice->CreateVertexShader(oVs.pData, oVs.zSize, nullptr, pRenderState->m_pVertexShader.ReleaseAndGetAddressOf())))
    {
      return nullptr;
    }            
    if (!DX11_OK(s_oGlobalData.m_oWindow.m_pDevice->CreatePixelShader(oPs.pData, oPs.zSize, nullptr, pRenderState->m_pPixelShader.ReleaseAndGetAddressOf())))
    {
      return nullptr;
    }      

    if (!DX11_OK(D3DReflect(oVs.pData, oVs.zSize, IID_ID3D11ShaderReflection, (void**)pRenderState->m_pVertexReflection.ReleaseAndGetAddressOf())))
    {
      return nullptr;
    }
    if (!DX11_OK(D3DReflect(oPs.pData, oPs.zSize, IID_ID3D11ShaderReflection, (void**)pRenderState->m_pPixelReflection.ReleaseAndGetAddressOf())))
    {
      return nullptr;
    }

    pRenderState->m_pGeometryShader = nullptr;

    if (!_oInfo.m_sGSFilename.empty())
    {
      size_t uSlashPosGS = _oInfo.m_sGSFilename.find_last_of('/');        
      size_t uDotPosGS = _oInfo.m_sGSFilename.find_last_of('.');
      std::string sGSPath(_oInfo.m_sGSFilename.substr(0, uSlashPosGS));
      sGSPath = sGSPath.empty() ? "" : sGSPath.substr(0, uSlashPosGS) + "/DX11";
      std::string sGSFilename(_oInfo.m_sGSFilename);
      sGSFilename = sGSFilename.empty() ? "" : sGSFilename.substr(uSlashPosGS, uDotPosGS - uSlashPosGS) + ".cso";

      ShaderCode oGs;
      if (!LoadShaderCode(_pAllocator, (sGSPath + sGSFilename).c_str(), oGs))
      {
        return nullptr;
      }

      if (!DX11_OK(s_oGlobalData.m_oWindow.m_pDevice->CreateGeometryShader(oGs.pData, oGs.zSize, nullptr, pRenderState->m_pGeometryShader.ReleaseAndGetAddressOf())))
      {
        return nullptr;
      }

      if (!DX11_OK(D3DReflect(oGs.pData, oGs.zSize, IID_ID3D11ShaderReflection, (void**)pRenderState->m_pGeometryReflection.ReleaseAndGetAddressOf())))
      {
        return nullptr;
      }
    }

    const D3D11_INPUT_ELEMENT_DESC oInputDesc[] =
    {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,							              D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,	   0,	D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    if (!DX11_OK(s_oGlobalData.m_oWindow.m_pDevice->CreateInputLayout(oInputDesc, 5u, oVs.pData, oVs.zSize, pRenderState->m_pInputLayout.ReleaseAndGetAddressOf())))
    {
      return nullptr;
    }

    D3D11_RASTERIZER_DESC oRasterizerDesc = {};
    oRasterizerDesc.FillMode = D3D11_FILL_SOLID;
    oRasterizerDesc.CullMode = GetD3D11CullMode(_oInfo.m_eCullMode);
    oRasterizerDesc.FrontCounterClockwise = true;
    oRasterizerDesc.DepthBias = 0;// DEPTH_BIAS_D32_FLOAT(-0.00001);
    oRasterizerDesc.DepthBiasClamp = 0;//-0.001;
    oRasterizerDesc.SlopeScaledDepthBias = 0;// 1.f;
    oRasterizerDesc.DepthClipEnable = true;
    oRasterizerDesc.MultisampleEnable = _uMsaaSamples > 1u;
    oRasterizerDesc.AntialiasedLineEnable = true;      

    if (!DX11_OK(s_oGlobalData.m_oWindow.m_pDevice->CreateRasterizerState(&oRasterizerDesc, pRenderState->m_pRasterizer.GetAddressOf())))
    {
      return nullptr;
    }

    //BlendState.RenderTarget[0].SrcBlend = (D3D11_BLEND)m_srcBlendFactor;
    //BlendState.RenderTarget[0].DestBlend = (D3D11_BLEND)m_dstBlendFactor;
    //BlendState.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    //BlendState.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    //BlendState.RenderTarget[0].BlendOp = (D3D11_BLEND_OP)m_blendOp;
    //BlendState.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    //BlendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    D3D11_BLEND_DESC oBlendDesc = {};
    oBlendDesc.AlphaToCoverageEnable = false;
    oBlendDesc.IndependentBlendEnable = false;
    oBlendDesc.RenderTarget[0].BlendEnable = _oInfo.m_bBlendEnabled;
    oBlendDesc.RenderTarget[0].BlendOp = GetD3D11BlendOp( _oInfo.m_eBlendOp);      
    oBlendDesc.RenderTarget[0].DestBlend = GetD3D11Blend(_oInfo.m_eDstBlendFactor);
    oBlendDesc.RenderTarget[0].SrcBlend = GetD3D11Blend(_oInfo.m_eSrcBlendFactor);
    oBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    oBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    oBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    oBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;            

    if (!DX11_OK(s_oGlobalData.m_oWindow.m_pDevice->CreateBlendState(&oBlendDesc, pRenderState->m_pBlendState.GetAddressOf())))
    {
      return nullptr;
    }

    D3D11_DEPTH_STENCIL_DESC oDSDesc = {};
    oDSDesc.DepthEnable = _oInfo.m_bDepthWrite || _oInfo.m_bDepthRead;      
    oDSDesc.DepthWriteMask = _oInfo.m_bDepthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;            

    if (_oInfo.m_bDepthRead)
    {
      oDSDesc.DepthFunc = GetD3D11DepthCompareFunc(_oInfo.m_eDepthCompareOp);
    }
    else
    {
      oDSDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    }
      

    if (!DX11_OK(s_oGlobalData.m_oWindow.m_pDevice->CreateDepthStencilState(&oDSDesc, pRenderState->m_pDSState.GetAddressOf())))
    {
      return nullptr;
    }

    return oGuard.Release();
  }

  void BeginRenderStateSetup(RenderState* /*_pRenderState*/)
  {
  }

  void EndRenderStateSetup(const Window* /*g_pWindow*/)
  {
  }

  void BindRenderState(RenderState* _pRenderState)
  {
    s_oGlobalData.m_oWindow.m_pContext->VSSetShader(_pRenderState->m_pVertexShader.Get(), nullptr, 0u);
    s_oGlobalData.m_oWindow.m_pContext->PSSetShader(_pRenderState->m_pPixelShader.Get(), nullptr, 0u);      
    s_oGlobalData.m_oWindow.m_pContext->GSSetShader(_pRenderState->m_pGeometryShader.Get(), nullptr, 0u);      
    s_oGlobalData.m_oWindow.m_pContext->IASetInputLayout(_pRenderState->m_pInputLayout.Get());
    s_oGlobalData.m_oWindow.m_pContext->RSSetState(_pRenderState->m_pRasterizer.Get());
    s_oGlobalData.m_oWindow.m_pContext->OMSetBlendState(_pRenderState->m_pBlendState.Get(), nullptr, 0xffffffff);
    s_oGlobalData.m_oWindow.m_pContext->OMSetDepthStencilState(_pRenderState->m_pDSState.Get(), 0);
  }

  void DestroyRenderState(RenderState* _pRenderState)
  {
    _pRenderState->~RenderState();
  }

  // Shader Reflection

  uint32_t GetConstantBufferCount(const RenderState* _pRenderState, PipelineStage _eStage)
  {
    uint32_t uCount = 0u;

    ID3D11ShaderReflection* pReflection = nullptr;

    switch (_eStage)
    {
    case STAGE_VERTEX:        
      pReflection = _pRenderState->m_pVertexReflection.Get();
      break;
    case STAGE_GEOM:
      pReflection = _pRenderState->m_pGeometryReflection.Get();
      break;
    case STAGE_PIXEL:
      pReflection = _pRenderState->m_pPixelReflection.Get();
      break;
    default:
      break;
    }

    if (pReflection)
    {
      D3D11_SHADER_DESC oDesc = {};
      pReflection->GetDesc(&oDesc);

      for(uint32_t i = 0; i < oDesc.ConstantBuffers; i++)
      {
        ID3D11ShaderReflectionConstantBuffer* pCBuffer = pReflection->GetConstantBufferByIndex(i);

        D3D11_SHADER_BUFFER_DESC oBufferDesc;
        pCBuffer->GetDesc(&oBufferDesc);

        //if (!IsGlobalResource(oBufferDesc.Name, _eStage))
        {
          uCount++;
        }
      }
    }

    return uCount;
  }

  uint32_t GetTextureCount(const RenderState* _pRenderState, PipelineStage _eStage)
  {      

    uint32_t uTextureCount = 0u;

    ID3D11ShaderReflection* pReflection = nullptr;

    switch (_eStage)
    {
    case STAGE_VERTEX:        
      pReflection  = _pRenderState->m_pVertexReflection.Get();
      break;
    case STAGE_GEOM:
      pReflection = _pRenderState->m_pGeometryReflection.Get();
      break;
    case STAGE_PIXEL:
      pReflection = _pRenderState->m_pPixelReflection.Get();
      break;
    default:
      break;
    }

    if (pReflection)
    {
      D3D11_SHADER_DESC oDesc = {};
      pReflection->GetDesc(&oDesc);

      for (unsigned int i = 0u; i < oDesc.BoundResources; i++)
      {
        D3D11_SHADER_INPUT_BIND_DESC oResourceDesc;
        pReflection->GetResourceBindingDesc(i, &oResourceDesc);

        if (oResourceDesc.Type == D3D_SIT_TEXTURE /* && !IsGlobalResource(oResourceDesc.Name, _eStage) */ )
        {
          uTextureCount++;;
        }
      }
    }

    return uTextureCount;
  }

  const char* GetConstantBufferName(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx)
  {
    D3D11_SHADER_BUFFER_DESC oDesc{};
      
    if(GetBufferByIndex(_pRenderState, _eStage, _uIdx, nullptr, oDesc))
    {
      return oDesc.Name;
    }
              
    else
    {
      return "[ERROR]";        
    }
  }

  const char* GetTextureName(const RenderState* /*_pRenderState*/, PipelineStage /*_eStage*/, uint32_t /*_uIdx*/)
  {
    return nullptr;
  }

  uint32_t GetConstantBufferMemberCount(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx)
  {
    D3D11_SHADER_BUFFER_DESC oDesc{};

    if (GetBufferByIndex(_pRenderState, _eStage, _uIdx, nullptr, oDesc))
    {
      return oDesc.Variables;
    }

    return 0u;
  }

  const char* GetConstantBufferMemberName(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx, uint32_t _uMemberIdx)
  {
    D3D11_SHADER_BUFFER_DESC oBufferDesc = {};

    ID3D11ShaderReflectionConstantBuffer* pCBuffer = nullptr;
      
    if (GetBufferByIndex(_pRenderState, _eStage, _uIdx, &pCBuffer, oBufferDesc))
    {
      if (_uMemberIdx < oBufferDesc.Variables)
      {
        if (ID3D11ShaderReflectionVariable* pVariable = pCBuffer->GetVariableByIndex(_uMemberIdx))
        {
          D3D11_SHADER_VARIABLE_DESC oVarDesc = {};
          pVariable->GetDesc(&oVarDesc);

          return oVarDesc.Name;
        }
      }
    }

    return "[ERROR]";
  }

  size_t GetConstantBufferMemberSize(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx, uint32_t _uMemberIdx)
  {
    D3D11_SHADER_BUFFER_DESC oBufferDesc = {};

    ID3D11ShaderReflectionConstantBuffer* pCBuffer = nullptr;

    if (GetBufferByIndex(_pRenderState, _eStage, _uIdx, &pCBuffer, oBufferDesc))
    {
      if (_uMemberIdx < oBufferDesc.Variables)
      {
        if (ID3D11ShaderReflectionVariable* pVariable = pCBuffer->GetVariableByIndex(_uMemberIdx))
        {
          D3D11_SHADER_VARIABLE_DESC oVarDesc = {};
          pVariable->GetDesc(&oVarDesc);

          return oVarDesc.Size;
        }
      }
    }

    return 0u;
  }

  ConstantBufferType GetConstantBufferMemberType(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx, uint32_t _uMemberIdx)
  {

    D3D11_SHADER_BUFFER_DESC oBufferDesc = {};

    ID3D11ShaderReflectionConstantBuffer* pCBuffer = nullptr;

    if (GetBufferByIndex(_pRenderState, _eStage, _uIdx, &pCBuffer, oBufferDesc))
    {
      if (_uMemberIdx < oBufferDesc.Variables)
      {
        if (ID3D11ShaderReflectionVariable* pVariable = pCBuffer->GetVariableByIndex(_uMemberIdx))
        {
          D3D11_SHADER_VARIABLE_DESC oVarDesc = {};
          pVariable->GetDesc(&oVarDesc);

          ID3D11ShaderReflectionType* pType = pVariable->GetType();
          D3D11_SHADER_TYPE_DESC oTypeDesc;
          pType->GetDesc(&oTypeDesc);

          switch (oTypeDesc.Columns)
          {
          case 1u:
            return ConstantBufferType::SCALAR;
          case 2u:
            return ConstantBufferType::VEC2;
          case 3u:
            if (oTypeDesc.Rows == 3u)
            {
              return ConstantBufferType::MAT3;
            }
            else if (oTypeDesc.Rows == 1u)
            {
              return ConstantBufferType::VEC3;
            }
          case 4u:
            if (oTypeDesc.Rows == 4u)
            {
              return ConstantBufferType::MAT4;
            }
            else if (oTypeDesc.Rows == 1u)
            {
              return ConstantBufferType::VEC4;
            }
          default:
            break;
          }
        }
      }
    }

    return ConstantBufferType::NONE;
  }

  uint32_t GetConstantBufferMemberArraySize(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx, uint32_t _uMemberIdx)
  {
    uint32_t uSize = 0u;

    D3D11_SHADER_BUFFER_DESC oBufferDesc = {};

    ID3D11ShaderReflectionConstantBuffer* pCBuffer = nullptr;

    if (GetBufferByIndex(_pRenderState, _eStage, _uIdx, &pCBuffer, oBufferDesc))
    {
      if (_uMemberIdx < oBufferDesc.Variables)
      {
        if (ID3D11ShaderReflectionVariable* pVariable = pCBuffer->GetVariableByIndex(_uMemberIdx))
        {
          D3D11_SHADER_VARIABLE_DESC oVarDesc = {};
          pVariable->GetDesc(&oVarDesc);

          ID3D11ShaderReflectionType* pType = pVariable->GetType();
          D3D11_SHADER_TYPE_DESC oTypeDesc;
          pType->GetDesc(&oTypeDesc);

          uSize = oTypeDesc.Elements;
        }
      }
    }

    if (uSize < 1)
    {
      uSize = 1;
    }

    return uSize;
  }

  size_t GetConstantBufferSize(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx)
  {
    D3D11_SHADER_BUFFER_DESC oDesc{};

    if (GetBufferByIndex(_pRenderState, _eStage, _uIdx, nullptr, oDesc))
    {
      return oDesc.Size;
    }

    return 0u;
  }

  size_t GetConstantBufferMemberOffset(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx, uint32_t _uMemberIdx)
  {
    D3D11_SHADER_BUFFER_DESC oBufferDesc = {};

    ID3D11ShaderReflectionConstantBuffer* pCBuffer = nullptr;

    if (GetBufferByIndex(_pRenderState, _eStage, _uIdx, &pCBuffer, oBufferDesc))
    {
      if (_uMemberIdx < oBufferDesc.Variables)
      {
        if (ID3D11ShaderReflectionVariable* pVariable = pCBuffer->GetVariableByIndex(_uMemberIdx))
        {
          D3D11_SHADER_VARIABLE_DESC oVarDesc = {};
          pVariable->GetDesc(&oVarDesc);

          return oVarDesc.StartOffset;
        }
      }
    }

    return 0u;
  }

  // Render substate

  ResourceSet* CreateResourceSet(LinearAllocator* _pAllocator, RenderState* _pRenderState, uint32_t /*_uSetId*/)
  {
    ResourceSet* pResourceSet = NewObject<ResourceSet>(_pAllocator);
    pResourceSet->m_pRenderState = _pRenderState;
    return pResourceSet;
  }

  void BeginResourceSetSetup(ResourceSet* _pSubState)
  {
    memset(_pSubState->m_lstTextures, 0, sizeof(_pSubState->m_lstTextures));
    memset(_pSubState->m_lstCBuffers, 0, sizeof(_pSubState->m_lstCBuffers));
  }

  bool ResourceSetAddConstantBuffer(ResourceSet* _pSubState, ConstantBuffer* _pCBuffer, size_t /*_uSize*/, const ResourceBindInfo& _oBindInfo)
  {           
    // Only one binding per cbuffer, so must be the same for all stages
    uint32_t uBinding = INVALID_BINDING_SLOT;

    if ((_oBindInfo.m_uStageFlags & STAGE_VERTEX) != 0u)
    {
      ID3D11ShaderReflection* pReflection = _pSubState->m_pRenderState->m_pVertexReflection.Get();
      uBinding = GetBindFromReflection(pReflection, _oBindInfo.m_sName, D3D_SIT_CBUFFER);
    }
    if (uBinding == INVALID_BINDING_SLOT && (_oBindInfo.m_uStageFlags & STAGE_GEOM) != 0u)
    {
      if (ID3D11ShaderReflection* pReflection = _pSubState->m_pRenderState->m_pGeometryReflection.Get())
      {
        uBinding = GetBindFromReflection(pReflection, _oBindInfo.m_sName, D3D_SIT_CBUFFER);
      }
    }
    if (uBinding == INVALID_BINDING_SLOT && (_oBindInfo.m_uStageFlags & STAGE_PIXEL) != 0u)
    {
      ID3D11ShaderReflection* pReflection = _pSubState->m_pRenderState->m_pPixelReflection.Get();
      uBinding = GetBindFromReflection(pReflection, _oBindInfo.m_sName, D3D_SIT_CBUFFER);
    }

    if (uBinding != INVALID_BINDING_SLOT)
    {
      ResourceSetCBufferBinding& rCBufferBinding = _pSubState->m_lstCBuffers[_pSubState->m_uCBufferBindingCount];
      rCBufferBinding.m_uBinding = uBinding;
      rCBufferBinding.m_uStageFlags = _oBindInfo.m_uStageFlags;
      rCBufferBinding.m_pConstantBuffer = _pCBuffer;

      _pSubState->m_uCBufferBindingCount++;
      return true;
    }

    LogError("[API] Cbuffer '%s' not found in the shaders", _oBindInfo.m_sName);
    return false;
  }

  bool ResourceSetAddTexture(ResourceSet* _pSubState, Texture* _pTexture, const ResourceBindInfo& _oBindInfo)
  {

    // Only one binding per texture, so must be the same for all stages
    uint32_t uBinding = INVALID_BINDING_SLOT;

    if ((_oBindInfo.m_uStageFlags & STAGE_VERTEX) != 0u)
    {
      ID3D11ShaderReflection* pReflection = _pSubState->m_pRenderState->m_pVertexReflection.Get();
      uBinding = GetBindFromReflection(pReflection, _oBindInfo.m_sName, D3D_SIT_CBUFFER);
    }
    if (uBinding == INVALID_BINDING_SLOT && (_oBindInfo.m_uStageFlags & STAGE_GEOM) != 0u)
    {
      if (ID3D11ShaderReflection* pReflection = _pSubState->m_pRenderState->m_pGeometryReflection.Get())
      {
        uBinding = GetBindFromReflection(pReflection, _oBindInfo.m_sName, D3D_SIT_CBUFFER);
      }
    }
    if (uBinding == INVALID_BINDING_SLOT && (_oBindInfo.m_uStageFlags & STAGE_PIXEL) != 0u)
    {
      ID3D11ShaderReflection* pReflection = _pSubState->m_pRenderState->m_pPixelReflection.Get();
      uBinding = GetBindFromReflection(pReflection, _oBindInfo.m_sName, D3D_SIT_CBUFFER);
    }

    if (uBinding != INVALID_BINDING_SLOT)
    {
      ResourceSetTextureBinding& rTextureBinding = _pSubState->m_lstTextures[_pSubState->m_uTextureBindingCount];
      rTextureBinding.m_uBinding = uBinding;
      rTextureBinding.m_uStageFlags = _oBindInfo.m_uStageFlags;
      rTextureBinding.m_pTexture = _pTexture;

      _pSubState->m_uTextureBindingCount++;
      return true;
    }

    LogError("[API] Texture '%s' not found in the shaders", _oBindInfo.m_sName);
    return false;
  }

  bool EndResourceSetSetup()
  {
    return true;
  }

  void BindResourceSet(RenderState* /*_pRenderState*/, ResourceSet* _pResourceSet)
  {
    for (uint32_t i = 0u; i < _pResourceSet->m_uCBufferBindingCount; i++)
    {
      ResourceSetCBufferBinding& rCBufferBinding = _pResourceSet->m_lstCBuffers[i];
      if ((rCBufferBinding.m_uStageFlags & STAGE_VERTEX) != 0u)
      {
        s_oGlobalData.m_oWindow.m_pContext->VSSetConstantBuffers(rCBufferBinding.m_uBinding, 1u, rCBufferBinding.m_pConstantBuffer->m_pCBuffer.GetAddressOf());        
      }
      if((rCBufferBinding.m_uStageFlags & STAGE_GEOM) != 0u)
      {
        s_oGlobalData.m_oWindow.m_pContext->GSSetConstantBuffers(rCBufferBinding.m_uBinding, 1u, rCBufferBinding.m_pConstantBuffer->m_pCBuffer.GetAddressOf());
      }
      if ((rCBufferBinding.m_uStageFlags & STAGE_PIXEL) != 0u)
      {
        s_oGlobalData.m_oWindow.m_pContext->PSSetConstantBuffers(rCBufferBinding.m_uBinding, 1u, rCBufferBinding.m_pConstantBuffer->m_pCBuffer.GetAddressOf());
      }
    }
    for (uint32_t i = 0u; i < _pResourceSet->m_uTextureBindingCount; i++)
    {
      ResourceSetTextureBinding& rTextureBinding = _pResourceSet->m_lstTextures[i];
      if ((rTextureBinding.m_uStageFlags & STAGE_VERTEX) != 0u)
      {
        s_oGlobalData.m_oWindow.m_pContext->VSSetShaderResources(rTextureBinding.m_uBinding, 1u, rTextureBinding.m_pTexture->m_pSRV.GetAddressOf());        
        s_oGlobalData.m_oWindow.m_pContext->VSSetSamplers(rTextureBinding.m_uBinding, 1u, rTextureBinding.m_pTexture->m_pSampler.GetAddressOf());

        s_oGlobalData.m_aVSBoundTextures[s_oGlobalData.m_uVSBoundTextureCount++] = rTextureBinding.m_pTexture;
      }
      if ((rTextureBinding.m_uStageFlags & STAGE_GEOM) != 0u)
      {
        s_oGlobalData.m_oWindow.m_pContext->GSSetShaderResources(rTextureBinding.m_uBinding, 1u, rTextureBinding.m_pTexture->m_pSRV.GetAddressOf());
        s_oGlobalData.m_oWindow.m_pContext->GSSetSamplers(rTextureBinding.m_uBinding, 1u, rTextureBinding.m_pTexture->m_pSampler.GetAddressOf());

        s_oGlobalData.m_aGSBoundTextures[s_oGlobalData.m_uGSBoundTextureCount++] = rTextureBinding.m_pTexture;
      }
      if ((rTextureBinding.m_uStageFlags & STAGE_PIXEL) != 0u)
      {
        s_oGlobalData.m_oWindow.m_pContext->PSSetShaderResources(rTextureBinding.m_uBinding, 1u, rTextureBinding.m_pTexture->m_pSRV.GetAddressOf());
        s_oGlobalData.m_oWindow.m_pContext->PSSetSamplers(rTextureBinding.m_uBinding, 1u, rTextureBinding.m_pTexture->m_pSampler.GetAddressOf());

        s_oGlobalData.m_aPSBoundTextures[s_oGlobalData.m_uPSBoundTextureCount++] = rTextureBinding.m_pTexture;
      }
    }
  }

  void DestroyResourceSet(ResourceSet* _pResourceSet)
  {
    _pResourceSet->~ResourceSet();
  }

  // Drawing

  void WaitForLastImage()
  {
    // We dont really need this feature, I think. DX11 should manage synchronization automatically
  }

  int BeginDraw()
  {
    if (s_oGlobalData.m_oWindow.m_bResize)
    {
      // ImGui's device objects (font texture, buffers, shaders) don't depend on the
      // swapchain, so they survive the resize untouched
      ResizeSwapchain();

      s_oGlobalData.m_oWindow.m_bResize = false;

      return 1;
    }

    return 0;
  }

  void BeginDrawOffline()
  {    
  }

  void DrawMesh(RenderState* /*_pRenderState*/, Mesh* _pMesh, uint32_t _uIndexCount, const void* /*_pConstantData*/, uint32_t /*_uConstantSize*/)
  {
    //UpdateConstantBuffer(g_pWindow, _pMesh->m_pModelCBuffer, _pConstantData, _uConstantSize);

    //BindConstantBuffer(g_pWindow, _pMesh->m_pModelCBuffer);

    uint32_t uStride = sizeof(Vertex);
    uint32_t uOffset = 0u;

    s_oGlobalData.m_oWindow.m_pContext->IASetVertexBuffers(0u, 1u, _pMesh->m_pVertexBuffer.GetAddressOf(), &uStride, &uOffset);
    s_oGlobalData.m_oWindow.m_pContext->IASetIndexBuffer(_pMesh->m_pIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0u);
    s_oGlobalData.m_oWindow.m_pContext->DrawIndexed(_uIndexCount, 0u, 0u);

  }

  void EndDraw()
  {
    // Nothing to do...
  }

  void Present()
  {
    if (s_oGlobalData.m_oWindow.m_pColorTexture)
    {
      ID3D11Texture2D* pBackBuffer = nullptr;
      DX11_CHECK_FATAL(s_oGlobalData.m_oWindow.m_pSwapchain->GetBuffer(0, __uuidof(ID3D11Resource), reinterpret_cast<void**>(&pBackBuffer)));

      s_oGlobalData.m_oWindow.m_pContext->ResolveSubresource(pBackBuffer, 0u, s_oGlobalData.m_oWindow.m_pColorTexture.Get(), 0u, s_oGlobalData.m_oWindow.m_eSwapchainFormat);

      pBackBuffer->Release();
    }

    DX11_CHECK_FATAL(s_oGlobalData.m_oWindow.m_pSwapchain->Present(1u, 0u));
  }

}
