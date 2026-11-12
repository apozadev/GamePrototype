#include "Game.h"

#include <cmath>

#include "Renderer.h"
#include "ResourceLoader.h"

//-----------------------------------------------------------------------------

// LoadResourceAsync copies the data, it doesn't need to outlive the call
static const Vertex s_aTriangleVerts[3] = {
  // pos               |     normal      |     tangent     |    color              |       uv
  {  0.0f,  0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
  {  0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f },
  { -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
};

static const uint16_t s_aTriangleIndices[3] = { 0, 1, 2 };

static ResourceHandle s_hTriangleMesh = INVALID_RESOURCE_HANDLE;
static ResourceHandle s_hTriangleMaterial = INVALID_RESOURCE_HANDLE;
static RenderObjectHandle s_hTriangle = 0;

static float s_fTime = 0.f;

//-----------------------------------------------------------------------------

void Game_Init(ThreadArenas* /*_pArenas*/)
{
  MeshLoadRequestInfo oMeshInfo = {};
  oMeshInfo.pVertexData = s_aTriangleVerts;
  oMeshInfo.zVertexDataSize = sizeof(s_aTriangleVerts);
  oMeshInfo.pIndexData = s_aTriangleIndices;
  oMeshInfo.zIndexDataSize = sizeof(s_aTriangleIndices);
  s_hTriangleMesh = LoadResourceAsync(oMeshInfo);

  MaterialLoadRequestInfo oMaterialInfo = {};
  // Relative to the executable: the platform layer sets the working directory,
  // and the build copies GamePrototype/assets next to the exe.
  oMaterialInfo.pVSFilename = "assets/shaders/triangleV.cso";
  oMaterialInfo.pPSFilename = "assets/shaders/triangleP.cso";
  oMaterialInfo.bBlendEnabled = false;
  s_hTriangleMaterial = LoadResourceAsync(oMaterialInfo);
}

void Game_Update(ThreadArenas* /*_pArenas*/, float _fDeltaTime)
{
  s_fTime += _fDeltaTime;

  MailboxHandle hMailbox = AcquireMailboxWrite();

  // Create the render object once its resources are on the GPU
  if (s_hTriangle == 0 &&
    GetResourceState(s_hTriangleMesh) == ResourceState::READY &&
    GetResourceState(s_hTriangleMaterial) == ResourceState::READY)
  {
    s_hTriangle = CreateRenderObject(hMailbox);
    SetRenderObjectMesh(hMailbox, s_hTriangle, s_hTriangleMesh);
    SetRenderObjectMaterial(hMailbox, s_hTriangle, s_hTriangleMaterial);
  }

  // Cycle through hues, one RGB channel 120 degrees apart from the next
  if (s_hTriangle != 0)
  {
    const float fPhase = 2.0943951f; // 2*pi/3
    const float vColor[4] = {
      0.5f + 0.5f * sinf(s_fTime),
      0.5f + 0.5f * sinf(s_fTime + fPhase),
      0.5f + 0.5f * sinf(s_fTime + 2.f * fPhase),
      1.f
    };
    SetMaterialProperty(hMailbox, s_hTriangleMaterial, "color", vColor, sizeof(vColor));
  }

  ReleaseMailboxWrite(hMailbox);
}

void Game_Shutdown(ThreadArenas* /*_pArenas*/)
{
  // Should we really do anything here?
}
