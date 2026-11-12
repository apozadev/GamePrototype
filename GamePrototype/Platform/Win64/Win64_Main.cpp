#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <thread>

#include "API/GraphicsAPI.h"
#include "Asserts.h"
#include "Memory/ThreadArenas.h"
#include "Platform/Debug.h"
#include "Renderer.h"
#include "ResourceLoader.h"

#include "../../Game.h"

static bool g_bRunning = true;

// The main thread outlives the render and loader threads, so its global arena
// also holds the memory they share (the load and create request rings).
static constexpr ThreadArenasConfig g_oMainThreadArenasConfig = {
  /*zGlobalSize=*/ 24u * 1024u * 1024u,
  /*zSceneSize=*/  16u * 1024u * 1024u,
  /*zFrameSize=*/  4u * 1024u * 1024u, // Reset before every Game_Update
};

static constexpr int g_iBackBufferWidth = 1280;
static constexpr int g_iBackBufferHeight = 720;

static const float g_fDeltaTime = 1.f / 60.f;

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
    // The render thread owns the swapchain, let it handle the resize
    RequestWindowResize();
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
  case WM_QUIT:
    g_bRunning = false;
    break;
  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYDOWN:
  case WM_KEYUP:
  {
    uint32_t uVKCode = (uint32_t)_wParam;

    uint32_t uKeyFlags = static_cast<uint32_t>(_lParam);
    bool bWasDown = (uKeyFlags & (1u << 30)) != 0;
    bool bIsDown = (uKeyFlags & (1u << 31)) == 0;

    [[maybe_unused]] bool bPress = bIsDown && !bWasDown;
    [[maybe_unused]] bool bRelease = !bIsDown && bWasDown;

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
    if (uVKCode == VK_LEFT)
    {
    }
    if (uVKCode == VK_ESCAPE)
    {
      if (bPress)
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
  // Assets are loaded relative to the executable, not to wherever it was launched from
  // (the VS debugger, for one, starts it in the build directory).
  {
    char aExePath[MAX_PATH];
    DWORD uLen = GetModuleFileNameA(NULL, aExePath, MAX_PATH);
    if (uLen > 0 && uLen < MAX_PATH)
    {
      char* pLastSlash = strrchr(aExePath, '\\');
      if (pLastSlash)
      {
        *pLastSlash = '\0';
        SetCurrentDirectoryA(aExePath);
      }
    }
  }

  // Before anything else: an escaped exception becomes a fatal error with a message.
  // The render and loader threads install their own.
  platform::InstallFatalErrorHandlersForThread();

  ThreadArenas* pArenas = CreateThreadArenas(g_oMainThreadArenasConfig);

  // Before any thread that uses them starts
  InitRenderer(&pArenas->oGlobal);
  InitResourceLoader(&pArenas->oGlobal);

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
  oWndClass.lpszClassName = "GamePrototypeWndClass";


  const ATOM uWndClass = RegisterClass(&oWndClass);
  FATAL_ASSERT_MSG_FORMAT(uWndClass != 0, "RegisterClass failed (error %lu)", GetLastError());

  HWND hWnd = CreateWindowEx(0,
    oWndClass.lpszClassName,
    "Actual Game",
    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    g_iBackBufferWidth,
    g_iBackBufferHeight,
    NULL,
    NULL,
    hInstance,
    NULL);

  FATAL_ASSERT_MSG_FORMAT(hWnd != NULL, "CreateWindowEx failed (error %lu)", GetLastError()); 

  const bool bAPIInitialized = api::Initialize();
  FATAL_ASSERT_MSG(bAPIInitialized, "Graphics API initialization failed (see log)");

  const bool bWindowCreated = api::CreateAPIWindow(&pArenas->oGlobal, hWnd, /*msaa=*/1);
  FATAL_ASSERT_MSG(bWindowCreated, "Graphics API window creation failed (see log)");

  // From here on, only the render thread touches the graphics API
  // (until it is joined below).
  std::thread oRenderThread(RenderThreadFnc);
  std::thread oLoaderThread(WorkerThreadProcessLoadRequests);

  Game_Init(pArenas);

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
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    
    pArenas->oFrame.Reset();
    Game_Update(pArenas, g_fDeltaTime);

    Sleep(16);     // just to slow down the loop a bit, not required            
    
    DWORD64 ulEndCycleCount = __rdtsc();
    //DWORD64 ulElapsedCycleCount = ulEndCycleCount - ulBeginCycleCount;
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
  Game_Shutdown(pArenas);

  RequestLoaderThreadExit();
  oLoaderThread.join();

  RequestRenderThreadExit();
  oRenderThread.join();

  // Render thread is gone, the main thread owns the API again
  api::DestroyWindow();
  api::ShutDown();

  // Both threads are joined: nothing references the request rings anymore
  DestroyThreadArenas(pArenas);

  return 0;
}