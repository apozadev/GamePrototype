#pragma once

#include "API/DX11/ConstantBuffer.h"

namespace api 
{
  struct Window;

  struct Mesh
  {

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_pVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_pIndexBuffer;

    unsigned int m_uIndexCount;

    //ConstantBuffer* m_pModelCBuffer;
  };
}
