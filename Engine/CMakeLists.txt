# Select exactly one rendering backend.
option(BUILD_DX11 "Build the DirectX 11 backend" OFF)
option(BUILD_VULKAN "Build the Vulkan backend" ON) 

if(BUILD_DX11 AND BUILD_VULKAN)
	message(FATAL_ERROR "Select only one rendering backend: BUILD_DX11 or BUILD_VULKAN")
elseif(NOT BUILD_DX11 AND NOT BUILD_VULKAN)
	message(FATAL_ERROR "Select a rendering backend: BUILD_DX11 or BUILD_VULKAN")
endif()

set(GRAPHICS_BACKEND_DIR "${CMAKE_CURRENT_SOURCE_DIR}/GraphicsBackend") 

set(ENGINE_SOURCES
	"${GRAPHICS_BACKEND_DIR}/Exception.cpp"
	"${GRAPHICS_BACKEND_DIR}/FileUtils.cpp"
	"${GRAPHICS_BACKEND_DIR}/InFile.cpp"
)

set(ENGINE_HEADERS
	"${GRAPHICS_BACKEND_DIR}/API/GraphicsAPI.h"
	"${GRAPHICS_BACKEND_DIR}/BlendEnums.h"
	"${GRAPHICS_BACKEND_DIR}/ConstantBufferTypes.h"
	"${GRAPHICS_BACKEND_DIR}/CullMode.h"
	"${GRAPHICS_BACKEND_DIR}/DepthCompareOp.h"
	"${GRAPHICS_BACKEND_DIR}/Exception.h"
	"${GRAPHICS_BACKEND_DIR}/FileUtils.h"
	"${GRAPHICS_BACKEND_DIR}/ImageFormat.h"
	"${GRAPHICS_BACKEND_DIR}/InFile.h"
	"${GRAPHICS_BACKEND_DIR}/LayoutConfig.h"
	"${GRAPHICS_BACKEND_DIR}/MeshConstant.h"
	"${GRAPHICS_BACKEND_DIR}/PipelineStage.h"
	"${GRAPHICS_BACKEND_DIR}/RenderStateInfo.h"
	"${GRAPHICS_BACKEND_DIR}/ResourceBindInfo.h"
	"${GRAPHICS_BACKEND_DIR}/SamplerConfig.h"
	"${GRAPHICS_BACKEND_DIR}/TextureSubResource.h"
	"${GRAPHICS_BACKEND_DIR}/TextureUsage.h"
	"${GRAPHICS_BACKEND_DIR}/TypedContiguousPool.h"
	"${GRAPHICS_BACKEND_DIR}/TypedPool.h"
	"${GRAPHICS_BACKEND_DIR}/Vertex.h"
)

if(BUILD_DX11)
	message(STATUS "Building with DirectX 11 backend")
	target_sources(ImGui PRIVATE "${PROJECT_SOURCE_DIR}/imgui/backends/imgui_impl_dx11.cpp")
	list(APPEND ENGINE_SOURCES
		"${GRAPHICS_BACKEND_DIR}/3rd/dxerr/dxerr.cpp"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/APIInternal.cpp"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/DX11API.cpp"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/DX11Exception.cpp"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/DX11Pools.cpp"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/RenderTargetBuilder.cpp"
	)
	list(APPEND ENGINE_HEADERS
		"${GRAPHICS_BACKEND_DIR}/API/DX11/APIInternal.h"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/ConstantBuffer.h"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/DX11BindSlotOffset.h"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/DX11Data.h"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/DX11Exception.h"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/DX11Macros.h"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/DX11Pools.h"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/Mesh.h"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/RenderState.h"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/RenderTarget.h"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/RenderTargetBuilder.h"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/ResourceSet.h"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/Texture.h"
		"${GRAPHICS_BACKEND_DIR}/API/DX11/Window.h"
	)
elseif(BUILD_VULKAN)
message(STATUS "Building with Vulkan backend")
	target_sources(ImGui PRIVATE "${PROJECT_SOURCE_DIR}/imgui/backends/imgui_impl_vulkan.cpp")
	list(APPEND ENGINE_SOURCES
		"${GRAPHICS_BACKEND_DIR}/3rd/spirv-reflect/spirv_reflect.cpp"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/APIInternal.cpp"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/DescriptorUtils.cpp"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/RenderTargetBuilder.cpp"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/VulkanAPI.cpp"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/VulkanException.cpp"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/VulkanPools.cpp"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/VulkanShaderReflection.cpp"
	)
	list(APPEND ENGINE_HEADERS
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/APIInternal.h"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/ConstantBuffer.h"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/DescriptorUtils.h"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/Mesh.h"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/RenderState.h"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/RenderTarget.h"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/RenderTargetBuilder.h"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/ResourceLayout.h"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/ResourceSet.h"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/Texture.h"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/VulkanData.h"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/VulkanException.h"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/VulkanMacros.h"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/VulkanPools.h"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/VulkanShaderReflection.h"
		"${GRAPHICS_BACKEND_DIR}/API/Vulkan/Window.h"
	)
endif()

add_library(Engine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})

target_include_directories(Engine PUBLIC "${GRAPHICS_BACKEND_DIR}")
target_compile_features(Engine PUBLIC cxx_std_17)
target_link_libraries(Engine PUBLIC ImGui)

if(BUILD_DX11)
	target_link_libraries(Engine PUBLIC d3d11 d3dcompiler dxguid dxgi)
elseif(BUILD_VULKAN)
	find_package(Vulkan REQUIRED)
	target_include_directories(ImGui PRIVATE ${Vulkan_INCLUDE_DIRS})
	target_link_libraries(Engine PUBLIC Vulkan::Vulkan)
endif()
