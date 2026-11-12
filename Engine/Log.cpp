#include "Log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

//-----------------------------------------------------------------------------

void LogError(const char* _sFormat, ...)
{
  char aBuffer[1024];

  int iPrefix = snprintf(aBuffer, sizeof(aBuffer), "[ERROR] ");

  va_list args;
  va_start(args, _sFormat);
  vsnprintf(aBuffer + iPrefix, sizeof(aBuffer) - iPrefix - 1, _sFormat, args);
  va_end(args);

  // Always end with a newline, even if the message was truncated
  size_t zLen = strnlen(aBuffer, sizeof(aBuffer) - 2);
  aBuffer[zLen] = '\n';
  aBuffer[zLen + 1] = '\0';

#ifdef _WIN32
  OutputDebugStringA(aBuffer);
#else
  fputs(aBuffer, stderr);
#endif
}
