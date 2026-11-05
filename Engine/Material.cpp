#include "Material.h"

#include <assert.h>
#include <cstring>

#include "Hash.h"

//-----------------------------------------------------------------------------

static MaterialConstantBuffer* FindConstantBuffer(Material* _pMaterial, const std::string& _sName)
{
  for (MaterialConstantBuffer& rCBuffer : _pMaterial->lstCBuffers)
  {
    if (rCBuffer.sName == _sName)
    {
      return &rCBuffer;
    }
  }
  return nullptr;
}

//-----------------------------------------------------------------------------

static void ReflectConstantBuffers(Material* _pMaterial, PipelineStage _eStage)
{
  const api::RenderState* pRenderState = _pMaterial->pRenderState;

  const uint32_t uCBufferCount = api::GetConstantBufferCount(pRenderState, _eStage);

  for (uint32_t i = 0u; i < uCBufferCount; i++)
  {
    std::string sName = api::GetConstantBufferName(pRenderState, _eStage, i);

    // Same cbuffer used by several stages: one buffer, bound to all of them.
    // Assumes every stage declares it with the same layout.
    if (MaterialConstantBuffer* pExisting = FindConstantBuffer(_pMaterial, sName))
    {
      assert(pExisting->lstData.size() == api::GetConstantBufferSize(pRenderState, _eStage, i));
      pExisting->uStageFlags |= _eStage;
      continue;
    }

    MaterialConstantBuffer oCBuffer = {};
    oCBuffer.sName = std::move(sName);
    oCBuffer.uStageFlags = _eStage;
    oCBuffer.lstData.resize(api::GetConstantBufferSize(pRenderState, _eStage, i), 0u);

    const uint32_t uMemberCount = api::GetConstantBufferMemberCount(pRenderState, _eStage, i);
    oCBuffer.lstProperties.reserve(uMemberCount);

    for (uint32_t j = 0u; j < uMemberCount; j++)
    {
      MaterialProperty oProperty = {};
      oProperty.sName = api::GetConstantBufferMemberName(pRenderState, _eStage, i, j);
      oProperty.uNameHash = HashName(oProperty.sName.c_str());
      oProperty.uOffset = static_cast<uint32_t>(api::GetConstantBufferMemberOffset(pRenderState, _eStage, i, j));
      oProperty.uSize = static_cast<uint32_t>(api::GetConstantBufferMemberSize(pRenderState, _eStage, i, j));

      assert(oProperty.uOffset + oProperty.uSize <= oCBuffer.lstData.size());

      oCBuffer.lstProperties.push_back(std::move(oProperty));
    }

    _pMaterial->lstCBuffers.push_back(std::move(oCBuffer));
  }
}

//-----------------------------------------------------------------------------

Material* CreateMaterial(const RenderStateInfo& _oInfo)
{
  Material* pMaterial = new Material();

  pMaterial->pRenderState = api::CreateRenderState(_oInfo, /*pRenderTarget=*/nullptr, api::GetWindowMSAASamples());

  ReflectConstantBuffers(pMaterial, STAGE_VERTEX);
  ReflectConstantBuffers(pMaterial, STAGE_PIXEL);
  if (!_oInfo.m_sGSFilename.empty())
  {
    ReflectConstantBuffers(pMaterial, STAGE_GEOM);
  }

  pMaterial->pResourceSet = api::CreateResourceSet(pMaterial->pRenderState, /*_uSetId=*/0);
  api::BeginResourceSetSetup(pMaterial->pResourceSet);

  for (MaterialConstantBuffer& rCBuffer : pMaterial->lstCBuffers)
  {
    rCBuffer.pCBuffer = api::CreateConstantBuffer(rCBuffer.lstData.size());

    ResourceBindInfo oBindInfo = {};
    oBindInfo.m_uStageFlags = rCBuffer.uStageFlags;
    oBindInfo.m_sName = rCBuffer.sName;
    api::ResourceSetAddConstantBuffer(pMaterial->pResourceSet, rCBuffer.pCBuffer, rCBuffer.lstData.size(), oBindInfo);
  }

  api::EndResourceSetSetup();

  return pMaterial;
}

//-----------------------------------------------------------------------------

void DestroyMaterial(Material* _pMaterial)
{
  api::DestroyResourceSet(_pMaterial->pResourceSet);

  for (MaterialConstantBuffer& rCBuffer : _pMaterial->lstCBuffers)
  {
    api::DestroyConstantBuffer(rCBuffer.pCBuffer);
  }

  api::DestroyRenderState(_pMaterial->pRenderState);

  delete _pMaterial;
}

//-----------------------------------------------------------------------------

bool SetMaterialProperty(Material* _pMaterial, uint32_t _uNameHash, const void* _pData, uint32_t _uSize)
{
  bool bFound = false;

  for (MaterialConstantBuffer& rCBuffer : _pMaterial->lstCBuffers)
  {
    for (const MaterialProperty& rProperty : rCBuffer.lstProperties)
    {
      if (rProperty.uNameHash == _uNameHash)
      {
        assert(_uSize <= rProperty.uSize && "Value is bigger than the shader property");
        const uint32_t uCopySize = (_uSize < rProperty.uSize) ? _uSize : rProperty.uSize;

        memcpy(rCBuffer.lstData.data() + rProperty.uOffset, _pData, uCopySize);
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
  for (MaterialConstantBuffer& rCBuffer : _pMaterial->lstCBuffers)
  {
    if (rCBuffer.bDirty)
    {
      api::UpdateConstantBuffer(rCBuffer.pCBuffer, rCBuffer.lstData.data(), rCBuffer.lstData.size());
      rCBuffer.bDirty = false;
    }
  }

  api::BindRenderState(_pMaterial->pRenderState);

  api::BindResourceSet(_pMaterial->pRenderState, _pMaterial->pResourceSet);
}
