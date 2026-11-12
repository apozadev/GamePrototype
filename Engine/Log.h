#pragma once

// Minimal error logging, safe from any thread.
// Goes to the debugger output window (OutputDebugString on Windows), so it
// still reports in builds where assert() is compiled out.
void LogError(const char* _sFormat, ...);
