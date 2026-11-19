# CMakeList.txt : CMake project for GamePrototype, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (GamePrototype WIN32
"Game.cpp"
"Platform/Win64/Win64_Main.cpp") 

# Link against Engine static library
target_link_libraries(GamePrototype PRIVATE Engine ImGui)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET GamePrototype PROPERTY CXX_STANDARD 20)
endif()

target_compile_options(GamePrototype PRIVATE /W4 /WX)

# Ship the assets next to the executable; the game loads them by relative path
add_custom_command(TARGET GamePrototype POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:GamePrototype>/assets"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PROJECT_SOURCE_DIR}/Engine/assets"
    "$<TARGET_FILE_DIR:GamePrototype>/assets/engine"
  COMMENT "Copying assets")
