#pragma once

#include <cstddef>

#include "Memory/LinearAllocator.h"

// The three lifetimes every thread allocates with. Each thread creates its own
// set at startup and passes it down explicitly; never share one across threads.
//
//   oGlobal: lives as long as the thread (or, for the main thread, the program)
//   oScene:  reset when the scene/level is unloaded
//   oFrame:  reset once per frame (per load request on the loader thread)
struct ThreadArenas
{
  LinearAllocator oGlobal;
  LinearAllocator oScene;
  LinearAllocator oFrame;
};

struct ThreadArenasConfig
{
  size_t zGlobalSize;
  size_t zSceneSize;
  size_t zFrameSize;
};

// One platform allocation holds the ThreadArenas struct and the three blocks.
// Fatal error if the OS can't provide the memory, so it never returns nullptr.
ThreadArenas* CreateThreadArenas(const ThreadArenasConfig& _oConfig);

// Releases the whole block. Anything still alive in the arenas must already be
// destroyed (GPU objects, objects with destructors).
void DestroyThreadArenas(ThreadArenas* _pArenas);
