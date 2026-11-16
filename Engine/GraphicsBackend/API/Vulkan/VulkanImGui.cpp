#include "API/ImGuiRenderer.h"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"

#include "Asserts.h"
#include "API/Vulkan/APIInternal.h"
#include "API/Vulkan/VulkanMacros.h"

namespace api
{
  // ImGui reports VkResults from its own calls through this. Every one of them is on a
  // path that must not fail (init, font upload, recording, buffer resizes).
  static void ImGuiCheckVkResult(VkResult _eResult)
  {
    if (_eResult != VK_SUCCESS)
    {
      VulkanFatal(_eResult, "Dear ImGui Vulkan backend call", __FILE__, __LINE__);
    }
  }

  void ImGuiRendererInit()
  {
    VkSurfaceCapabilitiesKHR oSurfaceCapabilities;
    VK_CHECK_FATAL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_oGlobalData.m_hPhysicalDevice, s_oGlobalData.m_oWindow.m_hSurface, &oSurfaceCapabilities));

    ImGui_ImplVulkan_InitInfo oImGuiInfo = {};
    oImGuiInfo.Instance = s_oGlobalData.m_hInstance;
    oImGuiInfo.PhysicalDevice = s_oGlobalData.m_hPhysicalDevice;
    oImGuiInfo.Device = s_oGlobalData.m_oWindow.m_hDevice;
    oImGuiInfo.QueueFamily = s_oGlobalData.m_uRenderQueueFamilyIdx;
    oImGuiInfo.Queue = s_oGlobalData.m_oWindow.m_hRenderQueue;
    oImGuiInfo.PipelineCache = VK_NULL_HANDLE;
    oImGuiInfo.DescriptorPool = s_oGlobalData.m_oWindow.m_hDescPool;
    oImGuiInfo.Subpass = 0;
    oImGuiInfo.MinImageCount = oSurfaceCapabilities.minImageCount;
    oImGuiInfo.ImageCount = s_oGlobalData.m_oWindow.m_uSwapchainImageCount;
    oImGuiInfo.MSAASamples = s_oGlobalData.m_oWindow.m_eMsaaSamples;
    oImGuiInfo.Allocator = nullptr;
    oImGuiInfo.CheckVkResultFn = ImGuiCheckVkResult;

    // The pipeline is built against the window render pass. RecreateSwapchain makes a new,
    // compatible one (same formats and samples), so the pipeline stays usable after a resize.
    const bool bInitialized = ImGui_ImplVulkan_Init(&oImGuiInfo, s_oGlobalData.m_oWindow.m_hRenderPass);
    FATAL_ASSERT_MSG(bInitialized, "[Vulkan] ImGui_ImplVulkan_Init failed");

    // Also builds the font atlas, which ImGui::NewFrame needs
    VkCommandBuffer hCmdBuffer = BeginTempCmdBuffer();
    const bool bFontsCreated = ImGui_ImplVulkan_CreateFontsTexture(hCmdBuffer);
    EndTempCmdBuffer(hCmdBuffer);
    FATAL_ASSERT_MSG(bFontsCreated, "[Vulkan] Could not create the ImGui font texture");

    ImGui_ImplVulkan_DestroyFontUploadObjects();
  }

  void ImGuiRendererShutdown()
  {
    // The last frames may still be reading ImGui's buffers and font texture
    VK_CHECK_FATAL(vkDeviceWaitIdle(s_oGlobalData.m_oWindow.m_hDevice));

    ImGui_ImplVulkan_Shutdown();
  }

  void ImGuiRendererDraw(ImDrawData* _pDrawData)
  {
    const uint32_t uFrameIdx = s_oGlobalData.m_oWindow.m_uCurrFrameIdx;
    ImGui_ImplVulkan_RenderDrawData(_pDrawData, s_oGlobalData.m_oWindow.m_pCmdBuffers[uFrameIdx]);
  }
}
