#pragma once

#include <windows.h>

namespace Platform::Testing
{
PROC WINAPI lookupVSyncFunction(LPCSTR name);
}

#define wglGetProcAddress Platform::Testing::lookupVSyncFunction
