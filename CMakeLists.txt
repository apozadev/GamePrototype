# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("GamePrototype")

# No exceptions anywhere: errors are return values, or fatal asserts (Engine/Asserts.h).
# Applied to every target (including third party) so the whole program agrees on the
# standard library configuration. With _HAS_EXCEPTIONS=0 the STL reports its own
# failures through a raise handler instead (see Engine/Platform/Debug.h).
if (MSVC)
  string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/EHs-c->)
  add_compile_definitions(_HAS_EXCEPTIONS=0)
endif()

# Dear ImGui
add_library(ImGui STATIC
  imgui/imgui.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  imgui/backends/imgui_impl_win32.cpp
)
target_include_directories(ImGui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
target_compile_features(ImGui PUBLIC cxx_std_20)

# Include sub-projects.
add_subdirectory ("Engine")
add_subdirectory ("GamePrototype")
