#pragma once

#include "TypedPool.h"

#include "API/DX11/RenderState.h"
#include "API/DX11/ConstantBuffer.h"
#include "API/DX11/Mesh.h"
#include "API/DX11/Texture.h"
#include "API/DX11/RenderTarget.h"

namespace api
{
  extern TypedPool<RenderState, 256>     s_oRenderStatePool;
  extern TypedPool<ConstantBuffer, 256>  s_oConstantBufferPool;
  extern TypedPool<Mesh, 256>            s_oMeshPool;
  extern TypedPool<Texture, 256>         s_oTexturePool;
  extern TypedPool<RenderTarget, 256>    s_oRenderTargetPool;
}
