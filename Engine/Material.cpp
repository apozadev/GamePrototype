#include "Material.h"

#include <cstring>

#include "Asserts.h"
#include "Hash.h"
#include "Log.h"
#include "Memory/LinearAllocator.h"

//-----------------------------------------------------------------------------

static const char* CopyString(LinearAllocator* _pAllocator, const char* _sString)
{
  const size_t zLength = strlen(_sString);
  char* pCopy = NewArray<char>(_pAllocator, zLength + 1u);
  memcpy(pCopy, _sString, zLength + 1u);
  return pCopy;
}

//-----------------------------------------------------------------------------

static MaterialConstantBuffer* FindConstantBuffer(Material* _pMaterial, const char* _sName)
{
  for (uint32_t i = 0u; i < _pMaterial->uCBufferCount; i++)
  {
    MaterialConstantBuffer& rCBuffer = _pMaterial->pCBuffers[i];
    if (strcmp(rCBuffer.sName, _sName) == 0)
    {
      return &rCBuffer;
    }
  }
  return nullptr;
}

//-----------------------------------------------------------------------------

// _pMaterial->pCBuffers must have room for every cbuffer of this stage.
// Returns false if the reflection data can't be read.
static bool ReflectConstantBuffers(LinearAllocator* _pAllocator, Material* _pMaterial, PipelineStage _eStage)
{
  const api::RenderState* pRenderState = _pMaterial->pRenderState;

  const uint32_t uCBufferCount = api::GetConstantBufferCount(pRenderState, _eStage);

  for (uint32_t i = 0u; i < uCBufferCount; i++)
  {
    const char* sName = api::GetConstantBufferName(pRenderState, _eStage, i);
    if (!sName)
    {
      return false;
    }

    // Same cbuffer used by several stages: one buffer, bound to all of them.
    // Assumes every stage declares it with the same layout.
    if (MaterialConstantBuffer* pExisting = FindConstantBuffer(_pMaterial, sName))
    {
      ASSERT_MSG_FORMAT(pExisting->uDataSize == api::GetConstantBufferSize(pRenderState, _eStage, i),
        "Cbuffer '%s' has a different layout in another stage", sName);
      pExisting->uStageFlags |= _eStage;
      continue;
    }

    MaterialConstantBuffer& rCBuffer = _pMaterial->pCBuffers[_pMaterial->uCBufferCount];
    rCBuffer.sName = CopyString(_pAllocator, sName);
    rCBuffer.uStageFlags = _eStage;
    rCBuffer.uDataSize = static_cast<uint32_t>(api::GetConstantBufferSize(pRenderState, _eStage, i));
    rCBuffer.pData = NewArray<uint8_t>(_pAllocator, rCBuffer.uDataSize);

    rCBuffer.uPropertyCount = api::GetConstantBufferMemberCount(pRenderState, _eStage, i);
    rCBuffer.pProperties = NewArray<MaterialProperty>(_pAllocator, rCBuffer.uPropertyCount);

    for (uint32_t j = 0u; j < rCBuffer.uPropertyCount; j++)
    {
      const char* sPropertyName = api::GetConstantBufferMemberName(pRenderState, _eStage, i, j);
      if (!sPropertyName)
      {
        return false;
      }

      MaterialProperty& rProperty = rCBuffer.pProperties[j];
      rProperty.sName = CopyString(_pAllocator, sPropertyName);
      rProperty.uNameHash = HashName(rProperty.sName);
      rProperty.uOffset = static_cast<uint32_t>(api::GetConstantBufferMemberOffset(pRenderState, _eStage, i, j));
      rProperty.uSize = static_cast<uint32_t>(api::GetConstantBufferMemberSize(pRenderState, _eStage, i, j));

      if (rProperty.uOffset + rProperty.uSize > rCBuffer.uDataSize)
      {
        LogError("[Material] Property '%s' lies outside cbuffer '%s'", rProperty.sName, rCBuffer.sName);
        return false;
      }
    }

    // Counted only once complete, so DestroyMaterial never sees a half-built entry
    _pMaterial->uCBufferCount++;
  }

  return true;
}

//-----------------------------------------------------------------------------

// Every step that can fail returns early; CreateMaterial cleans up after it
static bool BuildMaterial(LinearAllocator* _pAllocator, Material* _pMaterial, const RenderStateInfo& _oInfo)
{
  const bool bHasGS = !_oInfo.m_sGSFilename.empty();

  _pMaterial->pRenderState = api::CreateRenderState(_pAllocator, _oInfo, /*pRenderTarget=*/nullptr, api::GetWindowMSAASamples());
  if (!_pMaterial->pRenderState)
  {
    return false;
  }

  // Upper bound: cbuffers shared between stages are only stored once
  uint32_t uMaxCBufferCount = api::GetConstantBufferCount(_pMaterial->pRenderState, STAGE_VERTEX)
    + api::GetConstantBufferCount(_pMaterial->pRenderState, STAGE_PIXEL);
  if (bHasGS)
  {
    uMaxCBufferCount += api::GetConstantBufferCount(_pMaterial->pRenderState, STAGE_GEOM);
  }
  _pMaterial->pCBuffers = NewArray<MaterialConstantBuffer>(_pAllocator, uMaxCBufferCount);

  if (!ReflectConstantBuffers(_pAllocator, _pMaterial, STAGE_VERTEX)
    || !ReflectConstantBuffers(_pAllocator, _pMaterial, STAGE_PIXEL)
    || (bHasGS && !ReflectConstantBuffers(_pAllocator, _pMaterial, STAGE_GEOM)))
  {
    return false;
  }

  _pMaterial->pResourceSet = api::CreateResourceSet(_pAllocator, _pMaterial->pRenderState, /*_uSetId=*/0);
  if (!_pMaterial->pResourceSet)
  {
    return false;
  }

  api::BeginResourceSetSetup(_pMaterial->pResourceSet);

  for (uint32_t i = 0u; i < _pMaterial->uCBufferCount; i++)
  {
    MaterialConstantBuffer& rCBuffer = _pMaterial->pCBuffers[i];
    rCBuffer.pCBuffer = api::CreateConstantBuffer(_pAllocator, rCBuffer.uDataSize);
    if (!rCBuffer.pCBuffer)
    {
      return false;
    }

    ResourceBindInfo oBindInfo = {};
    oBindInfo.m_uStageFlags = rCBuffer.uStageFlags;
    oBindInfo.m_sName = rCBuffer.sName;
    if (!api::ResourceSetAddConstantBuffer(_pMaterial->pResourceSet, rCBuffer.pCBuffer, rCBuffer.uDataSize, oBindInfo))
    {
      return false;
    }
  }

  return api::EndResourceSetSetup();
}

//-----------------------------------------------------------------------------

Material* CreateMaterial(LinearAllocator* _pAllocator, const RenderStateInfo& _oInfo)
{
  const size_t zMark = _pAllocator->PeekCurrent();

  Material* pMaterial = NewObject<Material>(_pAllocator);

  if (!BuildMaterial(_pAllocator, pMaterial, _oInfo))
  {
    // Release whatever was created, then hand back everything allocated since zMark
    DestroyMaterial(pMaterial);
    _pAllocator->Rollback(zMark);
    return nullptr;
  }

  return pMaterial;
}

//-----------------------------------------------------------------------------

void DestroyMaterial(Material* _pMaterial)
{
  // Members can be null if CreateMaterial failed halfway
  if (_pMaterial->pResourceSet)
  {
    api::DestroyResourceSet(_pMaterial->pResourceSet);
  }

  for (uint32_t i = 0u; i < _pMaterial->uCBufferCount; i++)
  {
    MaterialConstantBuffer& rCBuffer = _pMaterial->pCBuffers[i];
    if (rCBuffer.pCBuffer)
    {
      api::DestroyConstantBuffer(rCBuffer.pCBuffer);
    }
  }

  if (_pMaterial->pRenderState)
  {
    api::DestroyRenderState(_pMaterial->pRenderState);
  }

  // Leave it inert in case anything still points to it
  *_pMaterial = {};
}

//-----------------------------------------------------------------------------

bool SetMaterialProperty(Material* _pMaterial, uint32_t _uNameHash, const void* _pData, uint32_t _uSize)
{
  bool bFound = false;

  for (uint32_t i = 0u; i < _pMaterial->uCBufferCount; i++)
  {
    MaterialConstantBuffer& rCBuffer = _pMaterial->pCBuffers[i];
    for (uint32_t j = 0u; j < rCBuffer.uPropertyCount; j++)
    {
      const MaterialProperty& rProperty = rCBuffer.pProperties[j];
      if (rProperty.uNameHash == _uNameHash)
      {
        ASSERT_MSG_FORMAT(_uSize <= rProperty.uSize, "Value for '%s' is bigger than the shader property", rProperty.sName);
        const uint32_t uCopySize = (_uSize < rProperty.uSize) ? _uSize : rProperty.uSize;

        memcpy(rCBuffer.pData + rProperty.uOffset, _pData, uCopySize);
        rCBuffer.bDirty = true;
        bFound = true;
        break;
      }
    }
  }

  return bFound;
}

//-----------------------------------------------------------------------------

void BindMaterial(Material* _pMaterial)
{
  for (uint32_t i = 0u; i < _pMaterial->uCBufferCount; i++)
  {
    MaterialConstantBuffer& rCBuffer = _pMaterial->pCBuffers[i];
    if (rCBuffer.bDirty)
    {
      api::UpdateConstantBuffer(rCBuffer.pCBuffer, rCBuffer.pData, rCBuffer.uDataSize);
      rCBuffer.bDirty = false;
    }
  }

  api::BindRenderState(_pMaterial->pRenderState);

  api::BindResourceSet(_pMaterial->pRenderState, _pMaterial->pResourceSet);
}
