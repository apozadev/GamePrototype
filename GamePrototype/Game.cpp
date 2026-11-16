#include "Game.h"

#include <cmath>

#include "imgui.h"

#include "Renderer.h"
#include "ResourceLoader.h"

//-----------------------------------------------------------------------------

static ResourceHandle s_hTriangleMesh = INVALID_RESOURCE_HANDLE;
static ResourceHandle s_hTriangleMaterial = INVALID_RESOURCE_HANDLE;
static RenderObjectHandle s_hTriangle = 0;

static float s_fTime = 0.f;
static bool s_bCycleColor = true;

//-----------------------------------------------------------------------------

void Game_Init(ThreadArenas* /*_pArenas*/)
{
  // Relative to the executable: the platform layer sets the working directory,
  // and the build copies GamePrototype/assets next to the exe.
  MeshLoadRequestInfo oMeshInfo = {};
  oMeshInfo.pFilename = "assets/models/triangle.obj";
  s_hTriangleMesh = LoadResourceAsync(oMeshInfo);

  MaterialLoadRequestInfo oMaterialInfo = {};
  oMaterialInfo.pVSFilename = "assets/shaders/triangleV.cso";
  oMaterialInfo.pPSFilename = "assets/shaders/triangleP.cso";
  oMaterialInfo.bBlendEnabled = false;
  s_hTriangleMaterial = LoadResourceAsync(oMaterialInfo);
}

void Game_Update(ThreadArenas* /*_pArenas*/, MailboxHandle _hMailbox, float _fDeltaTime)
{
  if (s_bCycleColor)
  {
    s_fTime += _fDeltaTime;
  }

  ImGui::Begin("Debug");
  ImGui::Text("%.1f FPS (%.2f ms)", ImGui::GetIO().Framerate, 1000.f / ImGui::GetIO().Framerate);
  ImGui::Checkbox("Cycle triangle color", &s_bCycleColor);
  ImGui::End();

  // Create the render object once its resources are on the GPU
  if (s_hTriangle == 0 &&
    GetResourceState(s_hTriangleMesh) == ResourceState::READY &&
    GetResourceState(s_hTriangleMaterial) == ResourceState::READY)
  {
    s_hTriangle = CreateRenderObject(_hMailbox);
    SetRenderObjectMesh(_hMailbox, s_hTriangle, s_hTriangleMesh);
    SetRenderObjectMaterial(_hMailbox, s_hTriangle, s_hTriangleMaterial);
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
    SetMaterialProperty(_hMailbox, s_hTriangleMaterial, "color", vColor, sizeof(vColor));
  }
}

void Game_Shutdown(ThreadArenas* /*_pArenas*/)
{
  // Should we really do anything here?
}
