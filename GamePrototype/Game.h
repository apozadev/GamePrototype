#pragma once

#include "Mailbox.h"

struct ThreadArenas;

// Game code runs on the main thread and allocates from the main thread's arenas.
// The frame arena is reset by the platform layer before every Game_Update.

void Game_Init(ThreadArenas* _pArenas);

// The platform layer owns the frame boundary: it acquires _hMailbox before the call and
// releases it after, and wraps the call in an ImGui frame, so ImGui:: widgets can be used
// anywhere in here. Render changes go through _hMailbox.
void Game_Update(ThreadArenas* _pArenas, MailboxHandle _hMailbox, float _fDeltaTime);

void Game_Shutdown(ThreadArenas* _pArenas);
