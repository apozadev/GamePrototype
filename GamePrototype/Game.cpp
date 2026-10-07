#include "Game.h"

#include "API/GraphicsAPI.h"

// simple constant structure for a 4×4 transform
struct MeshConstants { float mvp[16]; };

struct MaterialConstants { float color[4]; };

struct Vertex { float pos[3]; float normal[3]; float tangent[3]; float color[4]; float uv[2]; };

static api::RenderState* renderState = nullptr;

api::ResourceSet* resourceSet = nullptr;

api::ConstantBuffer* cbuffer = nullptr;

static api::Mesh* mesh = nullptr;

void Game_Init()
{
  // --- create a very simple render state that uses our shaders ---
  RenderStateInfo rsInfo{};
  rsInfo.m_sVSFilename = "D:/Documents/GraphicsBackend/apps/assets/shaders/triangleV.cso";
  rsInfo.m_sPSFilename = "D:/Documents/GraphicsBackend/apps/assets/shaders/triangleP.cso";
  //rsInfo.m_sVSFilename = "D:/Documents/GraphicsBackend/apps/assets/shaders/triangleV.spv";
  //rsInfo.m_sPSFilename = "D:/Documents/GraphicsBackend/apps/assets/shaders/triangleP.spv";

  rsInfo.m_uMeshConstantSize = sizeof(MeshConstants);
  rsInfo.m_bBlendEnabled = false;
  rsInfo.m_bDepthWrite = false;
  rsInfo.m_bDepthRead = false;
  rsInfo.m_eCullMode = CullMode::NONE;
  rsInfo.m_eDepthCompareOp = DepthCompareOp::ALWAYS;

  renderState = api::CreateRenderState(rsInfo, /*pRenderTarget=*/nullptr, api::GetWindowMSAASamples());

  ResourceBindInfo bindInfo{};
  bindInfo.m_sName = "MaterialConstants";
  bindInfo.m_uStageFlags = PipelineStage::STAGE_PIXEL;

  MaterialConstants materialConstants = { 1.0f, 0.0f, 0.0f, 1.0f };     // red color

  cbuffer = api::CreateConstantBuffer(sizeof(MaterialConstants));
  api::UpdateConstantBuffer(cbuffer, &materialConstants, sizeof(MaterialConstants));

  resourceSet = api::CreateResourceSet(renderState, /*_uSetId=*/0);
  api::BeginResourceSetSetup(resourceSet);
  api::ResourceSetAddConstantBuffer(resourceSet, cbuffer, sizeof(MaterialConstants), bindInfo);
  api::EndResourceSetSetup();

  // --- build a triangle mesh ---
  Vertex verts[3] = {
    // pos               |     normal      |     tangent     |    color              |       uv         
    {  0.0f,  0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    {  0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f },
    { -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
  };
  uint16_t indices[3] = { 0,1,2 };

  mesh = api::CreateMesh(verts, sizeof(verts),
                        indices, sizeof(indices),
                        sizeof(MeshConstants));
}

void Game_UpdateAndRender(float /*_fDeltaTime*/)
{
  // begin a frame, then bind state/mesh and draw
  if (api::BeginDraw() == 0)       // returns 1 when swapchain rebuild is required
  {
    api::ClearDefaultRenderTarget();

    api::BindDefaultRenderTarget();

    api::BindRenderState(renderState);

    api::BindResourceSet(renderState, resourceSet);

    MeshConstants constants = { /* compute MVP here */ };
    api::DrawMesh(renderState,
      mesh,
      /*indexCount=*/3,
      &constants,
      sizeof(constants));

    api::UnbindDefaultRenderTarget();

    api::EndDraw();
    api::Present();    
  }
}

void Game_Shutdown()
{
  // Should we really do anything here?
  api::DestroyMesh(mesh);
  api::DestroyConstantBuffer(cbuffer);
  api::DestroyResourceSet(resourceSet);
  api::DestroyRenderState(renderState);
  api::DestroyWindow();
  api::ShutDown();
}
