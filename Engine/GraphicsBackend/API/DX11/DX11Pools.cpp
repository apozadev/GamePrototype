#include "API/DX11/DX11Pools.h"

namespace api
{

  TypedPool<RenderState, 256>     s_oRenderStatePool;
  TypedPool<ConstantBuffer, 256>  s_oConstantBufferPool;
  TypedPool<Mesh, 256>            s_oMeshPool;
  TypedPool<Texture, 256>         s_oTexturePool;
  TypedPool<RenderTarget, 256>    s_oRenderTargetPool;

}