#pragma once

#include <string>

#include "BlendEnums.h"
#include "CullMode.h"
#include "DepthCompareOp.h"

struct RenderStateInfo
{  
  uint32_t m_uMeshConstantSize;

  std::string m_sPSFilename;
  std::string m_sGSFilename;
  std::string m_sVSFilename;

  bool m_bBlendEnabled;
  BlendOp m_eBlendOp;
  BlendFactor m_eSrcBlendFactor;
  BlendFactor m_eDstBlendFactor;

  bool m_bDepthWrite;
  bool m_bDepthRead;

  CullMode m_eCullMode;

  DepthCompareOp m_eDepthCompareOp;
};
