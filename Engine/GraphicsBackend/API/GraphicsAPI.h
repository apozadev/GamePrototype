#pragma once

#include <inttypes.h>

#include "../ImageFormat.h"
#include "../TextureUsage.h"
#include "../ResourceBindInfo.h"
#include "../RenderStateInfo.h"
#include "../PipelineStage.h"
#include "../ConstantBufferTypes.h"
#include "../SamplerConfig.h"
#include "../TextureSubResource.h"

namespace api 
{ 
  // Forward declarations for opaque pointers
  struct Mesh;
  struct ConstantBuffer;
  struct RenderState;
  struct ResourceSet;
	struct ResourceLayout;
  struct Texture;
  struct RenderTarget;  

	// General

	void Initialize();

	void ShutDown();

	uint32_t GetMaxMsaaSamples();

	// Window

	void CreateAPIWindow(HWND _hwnd, uint32_t _uMsaaSamples);

	void OnWindowResize();

	uint32_t GetWindowWidth();

	uint32_t GetWindowHeight();

	uint32_t GetWindowMSAASamples();

	void ClearDefaultRenderTarget();

	void BindDefaultRenderTarget();

	void UnbindDefaultRenderTarget();

	void DestroyWindow();

	// Mesh

	Mesh* CreateMesh(const void* _pVertexData, size_t _uVertexDataSize, const void* _pIndexData, size_t _uIndexDataSize, size_t _uMeshConstantSize);

	void DestroyMesh(Mesh* _pMesh);

	// Constant buffer

	ConstantBuffer* CreateConstantBuffer(size_t _uSize);

	void UpdateConstantBuffer(ConstantBuffer* _pCBuffer, const void* _pData, size_t _uSize);	

	void DestroyConstantBuffer(ConstantBuffer* _pCBuffer);

	// Texture

	Texture* CreateTexture(const void* const* _ppData, uint32_t _uWidth, uint32_t _uHeight, ImageFormat _eFormat, uint32_t _uMipLevels, uint32_t _uMsaaSamples, uint32_t _uUsage, const SamplerConfig& _rSamplerConfig, bool _bIsCubemap);

	void GenerateMipMaps(Texture* _pTexture);

	void ClearTexture(Texture* _pTexture, TextureUsage _eUsage);

	void BlitTexture(Texture* _pSrcTexture, const TextureSubResource& _rSrcSubResource, Texture* _pDstTexture, const TextureSubResource& _rDstSubResource);

	void DestroyTexture(Texture* _pTexture);

	// RenderTarget

	RenderTarget* CreateRenderTarget();

	void BeginRenderTargetSetup(RenderTarget* _pRenderTarget, uint32_t _uWidth, uint32_t _uHeight, ImageFormat _eFormat, ImageFormat _eDepthStencilFormat, uint32_t _uMsaaSamples, bool _bIsCubemap);

	void RenderTargetAddColorTexture(Texture* _pTexture, uint32_t _uMipLevel);

	void RenderTargetSetDepthStencilTexture(Texture* _pTexture, uint32_t _uMipLevel);

	void RenderTargetAddColorResolveTexture(Texture* _pTexture);

	void EndRenderTargetSetup();

	void ClearRenderTarget(RenderTarget* _pRenderTarget);

	void BindRenderTarget(RenderTarget* _pRenderTarget);

	void UnbindRenderTarget(RenderTarget* _pRenderTarget);

	void DestroyRenderTarget(RenderTarget* _pRenderTarget);

	// Render state

	RenderState* CreateRenderState(const RenderStateInfo& _oInfo, const RenderTarget* _pRenderTarget, uint32_t _uMsaaSamples);

	void BindRenderState(RenderState* _pRenderState);

	void DestroyRenderState(RenderState* _pRenderState);

	// Shader Reflection

	uint32_t GetConstantBufferCount(const RenderState* _pRenderState, PipelineStage _eStage);

	uint32_t GetTextureCount(const RenderState* _pRenderState, PipelineStage _eStage);

	std::string GetConstantBufferName(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx);

	const char* GetTextureName(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx);

	uint32_t GetConstantBufferMemberCount(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx);

	std::string GetConstantBufferMemberName(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx, uint32_t _uMemberIdx);

	size_t GetConstantBufferMemberSize(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx, uint32_t _uMemberIdx);

	ConstantBufferType GetConstantBufferMemberType(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx, uint32_t _uMemberIdx);

	uint32_t GetConstantBufferMemberArraySize(const RenderState* _pRenderState, PipelineStage _eStage, uint32_t _uIdx, uint32_t _uMemberIdx);

  // Resource set

	ResourceSet* CreateResourceSet(RenderState* _pRenderState, uint32_t _uSetId);

	void BeginResourceSetSetup(ResourceSet* _pRenderState);

	void ResourceSetAddConstantBuffer(ResourceSet* _pSubState, ConstantBuffer* _pCBuffer, size_t size, const ResourceBindInfo& _oBindInfo);

	void ResourceSetAddTexture(ResourceSet* _pSubState, Texture* _pTexture, const ResourceBindInfo& _oBindInfo);

	void EndResourceSetSetup();

	void BindResourceSet(RenderState* _pRenderState, ResourceSet* _pResourceSet);

	void DestroyResourceSet(ResourceSet* _pResourceSet);

	// Drawing

	void WaitForLastImage();

	int BeginDraw();

	void BeginDrawOffline();

	void DrawMesh(RenderState* _pRenderState, Mesh* _pMesh, uint32_t _uIndexCount, const void* _pConstantData, uint32_t _uConstantSize);

	void EndDraw();

	void Present();

	// Misc

	void ImGuiNewFrame();

}