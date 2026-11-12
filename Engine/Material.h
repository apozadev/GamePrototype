#pragma once

// Render thread only.
//
// A material is a render state plus every constant buffer its shaders declare,
// discovered through shader reflection. Properties are addressed by name, so
// no C++ struct has to mirror the shader's cbuffer layout.
//
// Plain data: a material and everything it owns (names, CPU cbuffer copies,
// API objects) live in the allocator passed to CreateMaterial.

#include <cstdint>

#include "API/GraphicsAPI.h"

struct LinearAllocator;

struct MaterialProperty
{
  const char* sName = nullptr; // Kept for debugging, lookups use uNameHash
  uint32_t uNameHash = 0u;
  uint32_t uOffset = 0u;  // Bytes from the start of the cbuffer, as packed by the shader compiler
  uint32_t uSize = 0u;
};

struct MaterialConstantBuffer
{
  const char* sName = nullptr;
  PipelineStageFlags uStageFlags = STAGE_NONE;
  api::ConstantBuffer* pCBuffer = nullptr;
  uint8_t* pData = nullptr; // CPU copy, uploaded to pCBuffer when dirty
  uint32_t uDataSize = 0u;
  MaterialProperty* pProperties = nullptr;
  uint32_t uPropertyCount = 0u;
  bool bDirty = true;
};

struct Material
{
  api::RenderState* pRenderState = nullptr;
  // TODO: Resource sets are a Vulkan concept leaking through the API
  api::ResourceSet* pResourceSet = nullptr;
  MaterialConstantBuffer* pCBuffers = nullptr;
  uint32_t uCBufferCount = 0u;
};

// Allocates everything from _pAllocator. If creation fails, whatever was created is
// destroyed, _pAllocator is rolled back to where it was, and it returns nullptr.
Material* CreateMaterial(LinearAllocator* _pAllocator, const RenderStateInfo& _oInfo);

// Releases the API objects. The memory goes back when its allocator is reset or rolled back.
void DestroyMaterial(Material* _pMaterial);

// Writes the value into every cbuffer of the material that declares the property.
// _uSize can be smaller than the property (e.g. first elements of an array).
// Returns false if no cbuffer declares it.
bool SetMaterialProperty(Material* _pMaterial, uint32_t _uNameHash, const void* _pData, uint32_t _uSize);

// Uploads dirty cbuffers and binds the material for drawing
void BindMaterial(Material* _pMaterial);
