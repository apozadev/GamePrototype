#pragma once

// Render thread only.
//
// A material is a render state plus every constant buffer its shaders declare,
// discovered through shader reflection. Properties are addressed by name, so
// no C++ struct has to mirror the shader's cbuffer layout.

#include <cstdint>
#include <string>
#include <vector>

#include "API/GraphicsAPI.h"

struct MaterialProperty
{
  std::string sName; // Kept for debugging, lookups use uNameHash
  uint32_t uNameHash;
  uint32_t uOffset;  // Bytes from the start of the cbuffer, as packed by the shader compiler
  uint32_t uSize;
};

struct MaterialConstantBuffer
{
  std::string sName;
  PipelineStageFlags uStageFlags = STAGE_NONE;
  api::ConstantBuffer* pCBuffer = nullptr;
  std::vector<uint8_t> lstData; // CPU copy, uploaded to pCBuffer when dirty
  std::vector<MaterialProperty> lstProperties;
  bool bDirty = true;
};

struct Material
{
  api::RenderState* pRenderState = nullptr;
  // TODO: Resource sets are a Vulkan concept leaking through the API
  api::ResourceSet* pResourceSet = nullptr;
  std::vector<MaterialConstantBuffer> lstCBuffers;
};

Material* CreateMaterial(const RenderStateInfo& _oInfo);

void DestroyMaterial(Material* _pMaterial);

// Writes the value into every cbuffer of the material that declares the property.
// _uSize can be smaller than the property (e.g. first elements of an array).
// Returns false if no cbuffer declares it.
bool SetMaterialProperty(Material* _pMaterial, uint32_t _uNameHash, const void* _pData, uint32_t _uSize);

// Uploads dirty cbuffers and binds the material for drawing
void BindMaterial(Material* _pMaterial);
