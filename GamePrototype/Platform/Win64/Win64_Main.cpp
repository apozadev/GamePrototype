#include <windows.h>
#include <stdint.h>

#include "API/GraphicsAPI.h"

#include "../../Game.h"

// TODO: addn glfw library

static bool g_bRunning = true;

static constexpr int g_iBackBufferWidth = 1280;
static constexpr int g_iBackBufferHeight = 720;

static const float g_fDeltaTime = 1.f / 60.f;

// simple constant structure for a 4×4 transform
struct MeshConstants { float mvp[16]; };

struct MaterialConstants { float color[4]; };

struct Vertex { float pos[3]; float normal[3]; float tangent[3]; float color[4]; float uv[2]; };

void GetWindowDimensions(HWND _hWnd, int& iWidth_, int& iHeight_)
{
  RECT oWindowRect;
  GetClientRect(_hWnd, &oWindowRect);
  iHeight_ = oWindowRect.bottom - oWindowRect.top;
  iWidth_ = oWindowRect.right - oWindowRect.left;
}

LRESULT WndProc(
  HWND _hWnd,
  UINT _uMsg,
  WPARAM _wParam,
  LPARAM _lParam)
{
  LRESULT result = 0;

  switch (_uMsg)
  {
  case WM_SIZE:
  {
    OutputDebugStringA("WM_SIZE\n");
  }
  break;
  case WM_DESTROY:
    OutputDebugStringA("WM_DESTROY\n");
    g_bRunning = false;
    break;
  case WM_CLOSE:
    OutputDebugStringA("WM_CLOSE\n");
    g_bRunning = false;
    break;
  case WM_ACTIVATEAPP:
    OutputDebugStringA("WM_ACTIVATEAPP\n");
    break;
  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYDOWN:
  case WM_KEYUP:
  {
    uint32_t uVKCode = (uint32_t)_wParam;

    bool bWasDown = (_lParam & (1 << 30)) != 0;
    bool bIsDown = (_lParam & (1 << 31)) != 0;

    bool bPress = bIsDown && !bWasDown;
    bool bRelease = !bIsDown && bWasDown;

    if (uVKCode == 'W')
    {
    }
    if (uVKCode == 'A')
    {
    }
    if (uVKCode == 'S')
    {
    }
    if (uVKCode == 'D')
    {
    }
    if (uVKCode == 'Q')
    {
    }
    if (uVKCode == 'E')
    {
    }
    if (uVKCode == VK_UP)
    {
    }
    if (uVKCode == VK_DOWN)
    {
    }
    if (uVKCode == VK_RIGHT)
    {
    }
    if (uVKCode == VK_UP)
    {
    }
    if (uVKCode == VK_ESCAPE)
    {
      if (bRelease)
      {
        g_bRunning = false;
      }
    }
    if (uVKCode == VK_SPACE)
    {
    }
  }
  break;
  default:
    result = DefWindowProc(_hWnd, _uMsg, _wParam, _lParam);
    break;
  }

  return result;
}

int WINAPI WinMain(
  HINSTANCE hInstance,
  HINSTANCE,
  LPSTR,
  int
)
{

  WNDCLASS oWndClass = {};
  oWndClass.style = CS_HREDRAW | CS_VREDRAW;
  oWndClass.lpfnWndProc = WndProc;
  oWndClass.cbClsExtra = 0;
  oWndClass.cbWndExtra = 0;
  oWndClass.hInstance = hInstance;
  oWndClass.hIcon = 0;
  oWndClass.hCursor = 0;
  oWndClass.hbrBackground = 0;
  oWndClass.lpszMenuName = NULL;
  oWndClass.lpszClassName = "HandmadeHeroWndClass";


  if (!RegisterClass(&oWndClass))
  {
    // ERROR: Window Class register failed
  }

  HWND hWnd = CreateWindowEx(0,
    oWndClass.lpszClassName,
    "HandmadeHero",
    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    g_iBackBufferWidth,
    g_iBackBufferHeight,
    NULL,
    NULL,
    hInstance,
    NULL);

  if (!hWnd)
  {
    // ERROR: Window creation failed
  } 

  api::Initialize();

  api::CreateAPIWindow(hWnd, /*msaa=*/1);

  // --- create a very simple render state that uses our shaders ---
  RenderStateInfo rsInfo{};
  //rsInfo.m_sVSFilename = "D:/Documents/GraphicsBackend/apps/assets/shaders/triangleV.cso";
  //rsInfo.m_sPSFilename = "D:/Documents/GraphicsBackend/apps/assets/shaders/triangleP.cso";
  rsInfo.m_sVSFilename = "D:/Documents/GraphicsBackend/apps/assets/shaders/triangleV.spv";
  rsInfo.m_sPSFilename = "D:/Documents/GraphicsBackend/apps/assets/shaders/triangleP.spv";

  rsInfo.m_uMeshConstantSize = sizeof(MeshConstants);
  rsInfo.m_bBlendEnabled = false;
  rsInfo.m_bDepthWrite = false;
  rsInfo.m_bDepthRead = false;
  rsInfo.m_eCullMode = CullMode::NONE;
  rsInfo.m_eDepthCompareOp = DepthCompareOp::ALWAYS;

  api::RenderState* renderState = api::CreateRenderState(rsInfo, /*pRenderTarget=*/nullptr, api::GetWindowMSAASamples());

  ResourceBindInfo bindInfo{};
  bindInfo.m_sName = "MaterialConstants";
  bindInfo.m_uStageFlags = PipelineStage::STAGE_PIXEL;

  MaterialConstants materialConstants = { 1.0f, 0.0f, 0.0f, 1.0f };     // red color

  api::ConstantBuffer* cbuffer = api::CreateConstantBuffer(sizeof(MaterialConstants));
  api::UpdateConstantBuffer(cbuffer, &materialConstants, sizeof(MaterialConstants));

  api::ResourceSet* resourceSet = api::CreateResourceSet(renderState, /*_uSetId=*/0);
  api::BeginResourceSetSetup(resourceSet);
  api::ResourceSetAddConstantBuffer(resourceSet, cbuffer, sizeof(MaterialConstants), bindInfo);
  api::EndResourceSetSetup();

  // --- build a triangle mesh ---
  Vertex verts[3] = {
    // pos               |     normal     |     tangent     |    color        |       uv         
    {  0.0f,  0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    {  0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f },
    { -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
  };
  uint16_t indices[3] = { 0,1,2 };

  api::Mesh* mesh =
    api::CreateMesh(verts, sizeof(verts),
      indices, sizeof(indices),
      sizeof(MeshConstants));

  Game_Init();

  LARGE_INTEGER ilPerfFrequency;
  QueryPerformanceFrequency(&ilPerfFrequency);

  LARGE_INTEGER ilBeginTime;
  QueryPerformanceCounter(&ilBeginTime);

  DWORD64 ulBeginCycleCount = __rdtsc();

  while (g_bRunning)
  {
    MSG msg;
    while (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_QUIT)
      {
        g_bRunning = false;
      }

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    
    Game_UpdateAndRender(g_fDeltaTime);

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

      Sleep(32);     // just to slow down the loop a bit, not required            
    }

    //HDC hDeviceCtx = GetDC(hWnd);

    //int iWidth, iHeight;
    //GetWindowDimensions(hWnd, iWidth, iHeight);

    //ReleaseDC(hWnd, hDeviceCtx);

    DWORD64 ulEndCycleCount = __rdtsc();
    DWORD64 ulElapsedCycleCount = ulEndCycleCount - ulBeginCycleCount;
    ulBeginCycleCount = ulEndCycleCount;

    LARGE_INTEGER ilEndTime;
    QueryPerformanceCounter(&ilEndTime);

    LONGLONG ilElapsedTime = ilEndTime.QuadPart - ilBeginTime.QuadPart;
    if (ilElapsedTime > 0)
    {
      ilElapsedTime = 1000 * ilElapsedTime / ilPerfFrequency.QuadPart;
    }

    LONGLONG ilFPS = 999999;

    if (ilElapsedTime > 0)
    {
      ilFPS = 1000 / ilElapsedTime;
    }

    //char aBuffer[256];
    //wsprintf(aBuffer, "FPS: %ld \t CPF: %ld\n", static_cast<long long>(ilFPS), static_cast<long long>(ulElapsedCycleCount / (1000 * 1000)));
    //OutputDebugStringA(aBuffer);

    ilBeginTime = ilEndTime;
  }

  // cleanup
  api::DestroyMesh(mesh);
  api::DestroyConstantBuffer(cbuffer);
  api::DestroyResourceSet(resourceSet);
  api::DestroyRenderState(renderState);
  api::DestroyWindow();
  api::ShutDown();

  return 0;
}