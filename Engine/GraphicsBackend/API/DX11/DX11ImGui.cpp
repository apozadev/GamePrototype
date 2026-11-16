#include "API/ImGuiRenderer.h"

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"

#include "Asserts.h"
#include "API/DX11/APIInternal.h"

namespace api
{
  void ImGuiRendererInit()
  {
    const bool bInitialized = ImGui_ImplDX11_Init(s_oGlobalData.m_oWindow.m_pDevice.Get(), s_oGlobalData.m_oWindow.m_pContext.Get());
    FATAL_ASSERT_MSG(bInitialized, "[DX11] ImGui_ImplDX11_Init failed");

    // ImGui_ImplDX11_NewFrame would create these lazily, but it is never called: the
    // ImGui frame runs on the update thread, which must not touch the device. Creating
    // them here also builds the font atlas before the first ImGui::NewFrame.
    const bool bDeviceObjectsCreated = ImGui_ImplDX11_CreateDeviceObjects();
    FATAL_ASSERT_MSG(bDeviceObjectsCreated, "[DX11] Could not create the ImGui device objects");
  }

  void ImGuiRendererShutdown()
  {
    ImGui_ImplDX11_Shutdown();
  }

  void ImGuiRendererDraw(ImDrawData* _pDrawData)
  {
    ImGui_ImplDX11_RenderDrawData(_pDrawData);
  }
}
