# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & run

Windows-only C++20 project (MSVC, CMake). There are no tests and no separate linter: `/W4 /WX` on `Engine` and `GamePrototype` makes every warning a build error, so a clean build is the check.

```sh
# From a VS 2022 x64 Developer environment (presets use Ninja + cl.exe)
cmake --preset x64-debug            # or x64-release (RelWithDebInfo)
cmake --build out/build/x64-debug

# Run (it sets its own working directory to the exe's folder)
out/build/x64-debug/GamePrototype/GamePrototype.exe
```

- Graphics backend is a compile-time choice: `-DBUILD_DX11=ON` (default) or `-DBUILD_VULKAN=ON -DBUILD_DX11=OFF`; exactly one must be on. Vulkan needs the Vulkan SDK.
- Shaders are precompiled and the compiled outputs (`.cso` for DX11, `.spv` for Vulkan) are committed. After editing a `.hlsl`/`.glsl`, rerun `compileShaders.bat` in that shaders folder (`fxc` / `glslc`). Every shader exists in both HLSL and GLSL and both must be kept in sync.
- A post-build step copies `GamePrototype/assets` → `<exe dir>/assets` and `Engine/assets` → `<exe dir>/assets/engine`. Asset paths in code are relative to the exe (e.g. `assets/models/triangle.obj`, `assets/engine/shaders/errorV.cso`).
- `.gitignore` has `*.obj` (meant for MSVC objects), which also ignores `.obj` model files under `assets/models`; use `git add -f` for models.

## Layout

- `Engine/` — static library: renderer, resource loader, memory, platform layer, asserts.
- `Engine/GraphicsBackend/` — backend-agnostic types plus `API/GraphicsAPI.h` (the `api::` namespace, opaque handles). `API/DX11/` and `API/Vulkan/` each implement the same interface; only one is compiled. `3rd/` holds dxerr and spirv-reflect.
- `Engine/Platform/` — OS layer; one `*_win32.cpp` per header, selected in `Engine/CMakeLists.txt`.
- `GamePrototype/` — the executable: `Platform/Win64/Win64_Main.cpp` (WinMain, window, main loop) and `Game.cpp` (game code via `Game_Init` / `Game_Update` / `Game_Shutdown`).
- `imgui/` — vendored Dear ImGui; don't edit.

New source files must be added to the explicit lists in the relevant `CMakeLists.txt` (no globbing).

## Architecture

**Three threads, each owning its state:**
- **Main/update thread** (`WinMain`): pumps Win32 messages, runs the ImGui frame and `Game_Update`. Communicates with the renderer only through a **mailbox**.
- **Render thread** (`RenderThreadFnc` in `Renderer.cpp`): the only thread that touches the graphics API between thread start and join. Consumes mailboxes and resource create requests.
- **Loader thread** (`WorkerThreadProcessLoadRequests` in `ResourceLoader.cpp`): reads files from disk, parses them (e.g. `ParseObj`), then posts create requests to the render thread.

**Frame flow:** two double-buffered mailboxes (`Mailbox.h`). The update thread calls `AcquireMailboxWrite` (blocks if the render thread is still on that one, so it runs at most one frame ahead), writes trivially-copyable messages through the `Renderer.h` API (`CreateRenderObject`, `SetRenderObjectMesh`, `SetMaterialProperty`, `SubmitImGuiDrawData`, …), then `ReleaseMailboxWrite`. The render thread applies the messages and draws. ImGui draw data is deep-copied (`ImGuiDrawDataCopy`) because ImGui runs on the main thread while the backend draws on the render thread.

**Async resources:** `LoadResourceAsync` (update thread) returns a `ResourceHandle` right away and pushes onto the load request ring → loader thread → `RequestCreateMesh/Material` onto the create request ring → render thread creates GPU objects → `ResourceCompletedCallback` (READY) or `RequestResourceFailed` (FAILED). Render objects may reference handles that are still LOADING; they are drawn once both resources exist, and FAILED ones are replaced by the engine error mesh/material. Material properties set while LOADING are buffered and applied on creation.

**Materials** (`Material.h`) are built from shader reflection: constant buffers and their fields are discovered at creation time and set by name (hashed), so no C++ struct mirrors cbuffer layouts.

**Rings are single-producer/single-consumer** (`RingAllocator`): load ring = update thread → loader; create ring = loader → render thread. Don't call those request functions from other threads.

## Conventions

- **No exceptions** (`/EHs-c-`, `_HAS_EXCEPTIONS=0`) and no `assert()`. Use `Asserts.h`: `ASSERT*` is debug-only and its condition is *not evaluated* in release (no side effects; code after it must still handle failure), `FATAL_ASSERT*` / `FATAL_ERROR*` fire in every build. Log with `LogError` (`Log.h`).
- Error policy (see header of `GraphicsAPI.h`): resource creation/loading logs and returns a zero value (nullptr/false/0) so the resource becomes FAILED; startup-critical setup and per-frame operations are fatal.
- **Memory:** prefer arenas over the general heap. Each thread creates its own `ThreadArenas` (`oGlobal` / `oScene` / `oFrame` linear allocators, `Memory/ThreadArenas.h`) and passes allocators explicitly. `api::Create*` take the allocator the object lives in; `Destroy*` frees API objects but not memory, so destroy objects before resetting their arena. Never share an arena across threads.
- **Naming** (Hungarian-style): `g_` globals, `s_` file statics; type prefixes `b` bool, `u` unsigned, `i` int, `f` float, `z` size_t, `p` pointer, `h` handle, `o` struct/object, `a` array, `s` string, `v` vector, `e` enum; parameters start with `_` (`_pAllocator`), out-parameters end with `_` (`oMesh_`). Constants are `UPPER_SNAKE_CASE`.
- Header comments document threading ("Update thread only", "Render thread only") and lifetime contracts; keep them accurate when changing behavior.
