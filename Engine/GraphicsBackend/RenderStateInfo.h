#pragma once

#include <string>

#include "BlendEnums.h"
#include "CullMode.h"
#include "DepthCompareOp.h"

struct RenderStateInfo
{  
  uint32_t m_uMeshConstantSize;

  std::string_view m_sPSFilename;
  std::string_view m_sGSFilename;
  std::string_view m_sVSFilename;

  bool m_bBlendEnabled;
  BlendOp m_eBlendOp;
  BlendFactor m_eSrcBlendFactor;
  BlendFactor m_eDstBlendFactor;

  bool m_bDepthWrite;
  bool m_bDepthRead;

  CullMode m_eCullMode;

  DepthCompareOp m_eDepthCompareOp;
};
