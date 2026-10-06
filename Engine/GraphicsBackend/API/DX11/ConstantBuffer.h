#pragma once

#include <d3d11.h>
#include <wrl.h>

#include "PipelineStage.h"

namespace api
{
  struct ConstantBuffer
  {      
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_pCBuffer;        
  };
}