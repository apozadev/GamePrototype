# CMakeList.txt : CMake project for GamePrototype, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (GamePrototype WIN32
"Game.cpp" 
"Game.h" 
"Platform/Win64/Win64_Main.cpp") 

# Link against Engine static library
target_link_libraries(GamePrototype PRIVATE Engine ImGui)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET GamePrototype PROPERTY CXX_STANDARD 20)
endif()
