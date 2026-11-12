#pragma once

struct ThreadArenas;

// Game code runs on the main thread and allocates from the main thread's arenas.
// The frame arena is reset by the platform layer before every Game_Update.

void Game_Init(ThreadArenas* _pArenas);

void Game_Update(ThreadArenas* _pArenas, float _fDeltaTime);

void Game_Shutdown(ThreadArenas* _pArenas);
