#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <dxgi.h>
#include <cmath>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

#include "API/GraphicsAPI.h"

#include "FileUtils.h"
#include "InFile.h"
#include "BlendEnums.h"
#include "Exception.h"
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
#include "API/DX11/DX11Pools.h"

namespace wrl = Microsoft::WRL;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define CUSTOM_PROP_NAME "CustomWndProcData"

#define INVALID_BINDING_SLOT 0xFFFFFFFFu

namespace api
{    

  // Define the user data structure
  struct WndProcData
  {
    WNDPROC originalWndProc;  // The original WndProc
  };

  // Forward declaration of the custom WndProc
  LRESULT CALLBACK CustomWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

  void HookWndProc(HWND _hwnd)
  {

    // Allocate and set the WndProc data
    WndProcData* data = new WndProcData();
    data->originalWndProc = (WNDPROC)GetWindowLongPtr(_hwnd, GWLP_WNDPROC);

    // Set the custom data as a property of the window
    SetProp(_hwnd, CUSTOM_PROP_NAME, (HANDLE)data);

    // Hook the new WndProc
    SetWindowLongPtr(_hwnd, GWLP_WNDPROC, (LONG_PTR)CustomWndProc);
  }

  void RestoreOriginalWndProc(HWND _hwnd)
  {

    // Retrieve the custom WndProc data
    WndProcData* data = (WndProcData*)GetProp(_hwnd, CUSTOM_PROP_NAME);

    if (data)
    {
      // Restore the original WndProc
      SetWindowLongPtr(_hwnd, GWLP_WNDPROC, (LONG_PTR)data->originalWndProc);

      // Clean up the custom data
      RemoveProp(_hwnd, CUSTOM_PROP_NAME);
      delete data;
    }
  }

  LRESULT CALLBACK CustomWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
  {
    // Retrieve the custom WndProc data
    WndProcData* data = (WndProcData*)GetProp(hWnd, CUSTOM_PROP_NAME);

    // Forward the event to ImGui's WndProc handler
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    {
      return true;
    }

    // Call the original WndProc
    if (data && data->originalWndProc)
    {
      return CallWindowProc(data->originalWndProc, hWnd, msg, wParam, lParam);
    }

    // Default processing if no original WndProc exists
    return DefWindowProc(hWnd, msg, wParam, lParam);
  }
    
  // General

  void Initialize()
  {
    s_oGlobalData.m_uMaxMsaaSamples = 0u;
    s_oRenderStatePool.Initialize();
    s_oConstantBufferPool.Initialize();
    s_oMeshPool.Initialize();
    s_oTexturePool.Initialize();
    s_oRenderTargetPool.Initialize();         

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls  
  }

  void ShutDown()
  {

    ImGui::DestroyContext();

    s_oRenderStatePool.Clear();
    s_oConstantBufferPool.Clear();
    s_oMeshPool.Clear();
    s_oTexturePool.Clear();
    s_oRenderTargetPool.Clear();
  }

  uint32_t GetMaxMsaaSamples()
  {
    return s_oGlobalData.m_uMaxMsaaSamples;
  }

  // Window

  void CreateAPIWindow(HWND _hwnd, uint32_t _uMsaaSamples)
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

    // ImGui stuff
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(s_oGlobalData.m_oWindow.m_hWnd);
    ImGui_ImplDX11_Init(s_oGlobalData.m_oWindow.m_pDevice.Get(), s_oGlobalData.m_oWindow.m_pContext.Get());

    ImGui::GetIO().DisplaySize.x = static_cast<float>(s_oGlobalData.m_oWindow.m_uWidth);
    ImGui::GetIO().DisplaySize.y = static_cast<float>(s_oGlobalData.m_oWindow.m_uHeight);

    HookWndProc(s_oGlobalData.m_oWindow.m_hWnd);

    SamplerConfig oSamplerCfg = {};
    oSamplerCfg.m_eAddressMode = TextureAddressMode::CLAMP;
    oSamplerCfg.m_eMagFilterMode = TextureFilterMode::POINT;
    oSamplerCfg.m_eMinFilterMode = TextureFilterMode::POINT;
    oSamplerCfg.m_eMipmapFilterMode = TextureFilterMode::POINT;

    uint8_t aDummyBuffer[4] = { 255u, 20u, 0u, 255u };
    void* pDummyBuffer = &(aDummyBuffer[0]);
    s_oGlobalData.m_oWindow.m_pDummyTexture = CreateTexture(&pDummyBuffer, 1u, 1u, ImageFormat::R8G8B8A8, 1u, 1u, TextureUsage::SHADER_RESOURCE, oSamplerCfg, false);

#ifndef NDEBUG
    Microsoft::WRL::ComPtr<ID3D11Debug> d3dDebug;
    DX11_CHECK(s_oGlobalData.m_oWindow.m_pDevice.As(&d3dDebug))

    Microsoft::WRL::ComPtr<ID3D11InfoQueue> d3dInfoQueue;
    DX11_CHECK(d3dDebug.As(&d3dInfoQueue))
    D3D11_MESSAGE_ID hide[] =
    {
      D3D11_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET,
    };
    D3D11_INFO_QUEUE_FILTER filter;
    memset(&filter, 0, sizeof(filter));
    filter.DenyList.NumIDs = _countof(hide);
    filter.DenyList.pIDList = hide;
    d3dInfoQueue->AddStorageFilterEntries(&filter);
#endif
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

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    ID3D11RenderTargetView* pNullView =  nullptr;      
    s_oGlobalData.m_oWindow.m_pContext->OMSetRenderTargets(1, &pNullView, nullptr);
  }

  void DestroyWindow()
  {

    RestoreOriginalWndProc(s_oGlobalData.m_oWindow.m_hWnd);

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();       

    DestroyTexture(s_oGlobalData.m_oWindow.m_pDummyTexture);

    s_oGlobalData.m_oWindow.m_pRtv = nullptr;
    s_oGlobalData.m_oWindow.m_pDsv = nullptr;
    s_oGlobalData.m_oWindow.m_pColorTexture = nullptr;
    s_oGlobalData.m_oWindow.m_pDSTexture = nullptr;      
    s_oGlobalData.m_oWindow.m_pSwapchain = nullptr;      
    s_oGlobalData.m_oWindow.m_pContext = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Debug> pDebug;
    s_oGlobalData.m_oWindow.m_pDevice.As(&pDebug);
    pDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

    s_oGlobalData.m_oWindow.m_pDevice = nullptr;
  }

  // Mesh

  Mesh* CreateMesh(const void* _pVertexData, size_t _uVertexDataSize, const void* _pIndexData, size_t _uIndexDataSize, size_t _uMeshConstantSize)
  {

    Mesh* pMesh = s_oMeshPool.PullElement();

    // Vertex buffer

    D3D11_BUFFER_DESC oVBDesc = {};
    oVBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    oVBDesc.Usage = D3D11_USAGE_IMMUTABLE;
    oVBDesc.CPUAccessFlags = 0u;
    oVBDesc.MiscFlags = 0u;
    oVBDesc.ByteWidth = _uVertexDataSize;
    oVBDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA oVBData = {};
    oVBData.pSysMem = _pVertexData;

    DX11_CHECK(s_oGlobalData.m_oWindow.m_pDevice->CreateBuffer(&oVBDesc, &oVBData, pMesh->m_pVertexBuffer.GetAddressOf()))

    // Index buffer

    D3D11_BUFFER_DESC oIBDesc = {};
    oIBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    oIBDesc.Usage = D3D11_USAGE_IMMUTABLE;
    oIBDesc.CPUAccessFlags = 0u;
    oIBDesc.MiscFlags = 0u;
    oIBDesc.ByteWidth = _uIndexDataSize;
    oIBDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA oIBData = {};
    oIBData.pSysMem = _pIndexData;
    oIBData.SysMemPitch = 0;
    oIBData.SysMemSlicePitch = 0;

    DX11_CHECK(s_oGlobalData.m_oWindow.m_pDevice->CreateBuffer(&oIBDesc, &oIBData, pMesh->m_pIndexBuffer.GetAddressOf()))

    pMesh->m_uIndexCount = _uIndexDataSize / sizeof(uint16_t);

    // Model CBuffer
    /*pMesh->m_pModelCBuffer = CreateConstantBuffer(g_pWindow, _uMeshConstantSize);

    ResourceBindInfo oBindInfo = {};
    oBindInfo.m_uStageFlags = STAGE_VERTEX;
    oBindInfo.m_sName = "ModelBuffer";

    ResourceSetAddConstantBuffer(g_pWindow, pMesh->m_pModelCBuffer, _uMeshConstantSize, oBindInfo);*/

    return pMesh;
  }

  void DestroyMesh(Mesh* _pMesh)
  {
    //DestroyConstantBuffer(g_pWindow, _pMesh->m_pModelCBuffer);
    s_oMeshPool.ReturnElement(_pMesh);
  }

  // Constant buffer

  ConstantBuffer* CreateConstantBuffer(size_t _uSize)
  {
    ConstantBuffer* pCBuffer = s_oConstantBufferPool.PullElement();

    D3D11_BUFFER_DESC oDesc = {};
    oDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    oDesc.Usage = D3D11_USAGE_DEFAULT;//D3D11_USAGE_DYNAMIC;
    oDesc.CPUAccessFlags = 0u;// D3D11_CPU_ACCESS_WRITE;
    oDesc.MiscFlags = 0u;
    oDesc.ByteWidth = _uSize;
    oDesc.StructureByteStride = 0;

    DX11_CHECK(s_oGlobalData.m_oWindow.m_pDevice->CreateBuffer(&oDesc, NULL, pCBuffer->m_pCBuffer.GetAddressOf()));
      
    return pCBuffer;
  }

  void UpdateConstantBuffer(ConstantBuffer* _pCbuffer, const void* _pData, size_t _uSize)
  {            
    /*D3D11_MAPPED_SUBRESOURCE oMsr;
    DX11_CHECK(pWindow->m_pContext->Map(_pCBuffer->m_pCBuffer.Get(), 0u, D3D11_MAP_WRITE_DISCARD, 0u, &oMsr));

    memcpy(oMsr.pData, &_pData, _uSize);

    pWindow->m_pContext->Unmap(_pCBuffer->m_pCBuffer.Get(), 0u);*/

    s_oGlobalData.m_oWindow.m_pContext->UpdateSubresource(_pCbuffer->m_pCBuffer.Get(), 0u, nullptr, _pData, 0u, 0u);

  }

  void DestroyConstantBuffer(ConstantBuffer* _pCBuffer)
  {
    s_oConstantBufferPool.ReturnElement(_pCBuffer);
  }

  // Texture

  Texture* CreateTexture(const void* const* _ppData, uint32_t _uWidth, uint32_t _uHeight, ImageFormat _eFormat, uint32_t _uMipLevels, uint32_t _uMsaaSamples, uint32_t _uUsage, const SamplerConfig& _rSamplerConfig, bool _bIsCubemap)
  {
    Texture* pTexture = s_oTexturePool.PullElement();      

    uint32_t uPixelSize = GetImageFormatSize(_eFormat);

    uint32_t uMemPitch = uPixelSize * _uWidth;

    DXGI_FORMAT eDXGIFormat = GetDXGIFormat(_eFormat);
    DXGI_FORMAT eDXGISrvFormat = GetDXGISrvFormat(_eFormat);

    uint32_t uMaxMipLevels = static_cast<uint32_t>(std::floor(std::log2(max(_uWidth, _uWidth)))) + 1;
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

        DX11_CHECK(s_oGlobalData.m_oWindow.m_pDevice->CreateTexture2D(&oTexDesc, _ppData ? &aData[0] : nullptr, pTexture->m_pTexture.GetAddressOf()));
      }
      else
      {
        D3D11_SUBRESOURCE_DATA oData = {};
        oData.pSysMem = _ppData ? *_ppData : nullptr;
        oData.SysMemPitch = uMemPitch;

        DX11_CHECK(s_oGlobalData.m_oWindow.m_pDevice->CreateTexture2D(&oTexDesc, _ppData ? &oData : nullptr, pTexture->m_pTexture.GetAddressOf()));
      }
    }
    else
    {
      //GFX_THROW_INFO(GetDevice()->CreateTexture2D(&desc, NULL, m_texture.GetAddressOf()));
      DX11_CHECK(s_oGlobalData.m_oWindow.m_pDevice->CreateTexture2D(&oTexDesc, nullptr, pTexture->m_pTexture.GetAddressOf()));
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

    DX11_CHECK(s_oGlobalData.m_oWindow.m_pDevice->CreateShaderResourceView(pTexture->m_pTexture.Get(), &oSrvDesc, pTexture->m_pSRV.GetAddressOf()));

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

    return pTexture;
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

    if (_rSrcSubResource.m_uMipLevels != _rDstSubResource.m_uMipLevels || _rSrcSubResource.m_uLayers != _rDstSubResource.m_uLayers)
    {
      THROW_GENERIC_EXCEPTION("BlitTexture: Subresource Mip level count and layer count must match");
    }
    else if ((_rSrcSubResource.m_uBaseMip + _rSrcSubResource.m_uMipLevels) > _pSrcTexture->m_uMipLevels &&
      (_rDstSubResource.m_uBaseMip + _rDstSubResource.m_uMipLevels) > _pDstTexture->m_uMipLevels)
    {
      THROW_GENERIC_EXCEPTION("BlitTexture: Subresource Mip levels are outside of the texure mip level range");
    }
    else if ((_rSrcSubResource.m_uBaseLayer + _rSrcSubResource.m_uLayers) > _pSrcTexture->m_uLayers &&
      (_rDstSubResource.m_uBaseLayer + _rDstSubResource.m_uLayers) > _pDstTexture->m_uLayers)
    {
      THROW_GENERIC_EXCEPTION("BlitTexture: Subresource array layers are outside of the texure array layers range");
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
    s_oTexturePool.ReturnElement(_pTexture);
  }

  // RenderTarget

  RenderTarget* CreateRenderTarget()
  {
    RenderTarget* pRenderTarget = s_oRenderTargetPool.PullElement();

    return pRenderTarget;
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

  void EndRenderTargetSetup()
  {
    s_oGlobalData.m_oRenderTargetBuilder.Build();
    s_oGlobalData.m_oRenderTargetBuilder.Clear();
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
    for (int i = 0; i < s_oGlobalData.m_uVSBoundTextureCount; i++)
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
      s_oGlobalData.m_oWindow.m_pContext->OMSetRenderTargets(_pRenderTarget->m_lstRtv.size(), _pRenderTarget->m_lstRtv[0].GetAddressOf(), _pRenderTarget->m_pDsv.Get());
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
      const std::vector<ID3D11RenderTargetView*> lstNullViews(_pRenderTarget->m_lstRtv.size(), nullptr);
      s_oGlobalData.m_oWindow.m_pContext->OMSetRenderTargets(_pRenderTarget->m_lstRtv.size(), lstNullViews.data(), nullptr);
    }
    else
    {
      s_oGlobalData.m_oWindow.m_pContext->OMSetRenderTargets(0u, nullptr, nullptr);
    }
  }
    
  void DestroyRenderTarget(RenderTarget* _pRenderTarget)
  {
    s_oRenderTargetPool.ReturnElement(_pRenderTarget);
  }

  // Render state

  RenderState* CreateRenderState(const RenderStateInfo& _oInfo, const RenderTarget* /*_pRenderTarget*/, uint32_t _uMsaaSamples)
  {

    RenderState* pRenderState = s_oRenderStatePool.PullElement();

    file::InFile oVsFile(_oInfo.m_sVSFilename.c_str());
    file::InFile oPsFile(_oInfo.m_sPSFilename.c_str());
      
    DX11_CHECK(s_oGlobalData.m_oWindow.m_pDevice->CreateVertexShader(oVsFile.GetData(), oVsFile.GetSize(), nullptr, pRenderState->m_pVertexShader.ReleaseAndGetAddressOf()));            
    DX11_CHECK(s_oGlobalData.m_oWindow.m_pDevice->CreatePixelShader(oPsFile.GetData(), oPsFile.GetSize(), nullptr, pRenderState->m_pPixelShader.ReleaseAndGetAddressOf()));      

    DX11_CHECK(D3DReflect(oVsFile.GetData(), oVsFile.GetSize(), IID_ID3D11ShaderReflection, (void**)pRenderState->m_pVertexReflection.ReleaseAndGetAddressOf()));
    DX11_CHECK(D3DReflect(oPsFile.GetData(), oPsFile.GetSize(), IID_ID3D11ShaderReflection, (void**)pRenderState->m_pPixelReflection.ReleaseAndGetAddressOf()));

    pRenderState->m_pGeometryShader = nullptr;

    if (!_oInfo.m_sGSFilename.empty())
    {
      size_t uSlashPosGS = _oInfo.m_sGSFilename.find_last_of('/');        
      size_t uDotPosGS = _oInfo.m_sGSFilename.find_last_of('.');
      std::string sGSPath = _oInfo.m_sGSFilename.empty() ? "" : _oInfo.m_sGSFilename.substr(0, uSlashPosGS) + "/DX11";
      std::string sGSFilename = _oInfo.m_sGSFilename.empty() ? "" : _oInfo.m_sGSFilename.substr(uSlashPosGS, uDotPosGS - uSlashPosGS) + ".cso";

      file::InFile oGsFile(_oInfo.m_sGSFilename.empty() ? "" : (sGSPath + sGSFilename).c_str());

      DX11_CHECK(s_oGlobalData.m_oWindow.m_pDevice->CreateGeometryShader(oGsFile.GetData(), oGsFile.GetSize(), nullptr, pRenderState->m_pGeometryShader.ReleaseAndGetAddressOf()));

      DX11_CHECK(D3DReflect(oGsFile.GetData(), oGsFile.GetSize(), IID_ID3D11ShaderReflection, (void**)pRenderState->m_pGeometryReflection.ReleaseAndGetAddressOf()));
    }

    const D3D11_INPUT_ELEMENT_DESC oInputDesc[] =
    {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,							              D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,	   0,	D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    DX11_CHECK(s_oGlobalData.m_oWindow.m_pDevice->CreateInputLayout(oInputDesc, 5u, oVsFile.GetData(), oVsFile.GetSize(), pRenderState->m_pInputLayout.ReleaseAndGetAddressOf()))

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

    DX11_CHECK(s_oGlobalData.m_oWindow.m_pDevice->CreateRasterizerState(&oRasterizerDesc, pRenderState->m_pRasterizer.GetAddressOf()))

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

    DX11_CHECK(s_oGlobalData.m_oWindow.m_pDevice->CreateBlendState(&oBlendDesc, pRenderState->m_pBlendState.GetAddressOf()));

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
      

    DX11_CHECK(s_oGlobalData.m_oWindow.m_pDevice->CreateDepthStencilState(&oDSDesc, pRenderState->m_pDSState.GetAddressOf()));

    return pRenderState;
  }

  void BeginRenderStateSetup(RenderState* _pRenderState)
  {
  }

  void EndRenderStateSetup(const Window* g_pWindow)
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
    s_oRenderStatePool.ReturnElement(_pRenderState);
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

  std::string GetConstantBufferName(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx)
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

  const char* GetTextureName(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx)
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

  std::string GetConstantBufferMemberName(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx, uint32_t _uMemberIdx)
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

  // Render substate

  ResourceSet* CreateResourceSet(RenderState* _pRenderState, uint32_t /*_uSetId*/)
  {
    ResourceSet* pResourceSet = new ResourceSet();
    pResourceSet->m_pRenderState = _pRenderState;
    return pResourceSet;
  }

  void BeginResourceSetSetup(ResourceSet* _pSubState)
  {
    memset(_pSubState->m_lstTextures, 0, sizeof(_pSubState->m_lstTextures));
    memset(_pSubState->m_lstCBuffers, 0, sizeof(_pSubState->m_lstCBuffers));
  }

  void ResourceSetAddConstantBuffer(ResourceSet* _pSubState, ConstantBuffer* _pCBuffer, size_t /*_uSize*/, const ResourceBindInfo& _oBindInfo)
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
    }      
    else
    {
      THROW_GENERIC_EXCEPTION("[API] Cbuffer name not found")
    }
  }

  void ResourceSetAddTexture(ResourceSet* _pSubState, Texture* _pTexture, const ResourceBindInfo& _oBindInfo)
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
    }
    else
    {
      THROW_GENERIC_EXCEPTION("[API] Texture name not found")
    }    

  }

  void EndResourceSetSetup()
  {
  }

  void BindResourceSet(RenderState* _pRenderState, ResourceSet* _pResourceSet)
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
    delete _pResourceSet;
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
      ImGui_ImplDX11_InvalidateDeviceObjects();

      ResizeSwapchain();

      ImGui_ImplDX11_CreateDeviceObjects();

      ImGui::EndFrame();

      ImGui::GetIO().DisplaySize.x = static_cast<float>(s_oGlobalData.m_oWindow.m_uWidth);
      ImGui::GetIO().DisplaySize.y = static_cast<float>(s_oGlobalData.m_oWindow.m_uHeight);

      s_oGlobalData.m_oWindow.m_bResize = false;

      return 1;
    }            

    api::ImGuiNewFrame();
      
    return 0;
  }

  void BeginDrawOffline()
  {    
  }

  void DrawMesh(RenderState* _pRenderState, Mesh* _pMesh, uint32_t _uIndexCount, const void* _pConstantData, uint32_t _uConstantSize)
  {
    //UpdateConstantBuffer(g_pWindow, _pMesh->m_pModelCBuffer, _pConstantData, _uConstantSize);

    //BindConstantBuffer(g_pWindow, _pMesh->m_pModelCBuffer);

    uint32_t uStride = sizeof(Vertex);
    uint32_t uOffset = 0u;

    s_oGlobalData.m_oWindow.m_pContext->IASetVertexBuffers(0u, 1u, _pMesh->m_pVertexBuffer.GetAddressOf(), &uStride, &uOffset);
    s_oGlobalData.m_oWindow.m_pContext->IASetIndexBuffer(_pMesh->m_pIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0u);
    s_oGlobalData.m_oWindow.m_pContext->DrawIndexed(_pMesh->m_uIndexCount, 0u, 0u);

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
      DX11_CHECK(s_oGlobalData.m_oWindow.m_pSwapchain->GetBuffer(0, __uuidof(ID3D11Resource), reinterpret_cast<void**>(&pBackBuffer)));

      s_oGlobalData.m_oWindow.m_pContext->ResolveSubresource(pBackBuffer, 0u, s_oGlobalData.m_oWindow.m_pColorTexture.Get(), 0u, s_oGlobalData.m_oWindow.m_eSwapchainFormat);

      pBackBuffer->Release();
    }

    DX11_CHECK(s_oGlobalData.m_oWindow.m_pSwapchain->Present(1u, 0u))
  }

  // Misc

  void ImGuiNewFrame()
  {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
  }
  
}
