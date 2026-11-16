#pragma once

// The graphics half of Dear ImGui: draws ImDrawData with the backend's device.
// Everything else (context, input, NewFrame/Render) belongs to the platform layer
// and the update thread; the backend never sees a window message or an ImGui frame.
//
// Each backend implements these in its own file (DX11ImGui.cpp, VulkanImGui.cpp),
// so GraphicsAPI.h stays free of ImGui.

struct ImDrawData;

namespace api
{
  // Creates the device objects, font texture included (ImGui::NewFrame needs the font
  // atlas built). Call after CreateAPIWindow, once the ImGui context exists, and before
  // the render thread starts. Fatal on failure, like CreateAPIWindow.
  void ImGuiRendererInit();

  // Call after the render thread is joined and before DestroyWindow.
  void ImGuiRendererShutdown();

  // Render thread only, between BindDefaultRenderTarget and UnbindDefaultRenderTarget.
  // _pDrawData must stay valid and unchanged for the whole call: pass a copy, never
  // ImGui::GetDrawData() of a context the update thread is still using.
  void ImGuiRendererDraw(ImDrawData* _pDrawData);
}
