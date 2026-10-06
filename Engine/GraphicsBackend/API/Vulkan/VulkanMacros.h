#pragma once

#if _DEBUG

#include "API/Vulkan/VulkanException.h"

#define VK_CHECK(expr) { VkResult __hResult = expr; if(__hResult != VK_SUCCESS){ throw api::VulkanException(__LINE__,__FILE__, __hResult);}}

#else

#define VK_CHECK(expr) expr;

#endif