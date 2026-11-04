#pragma once

#include <windows.h>

namespace Platform::Testing
{
HWND WINAPI captureMouse(HWND window);
}

#define SetCapture Platform::Testing::captureMouse
