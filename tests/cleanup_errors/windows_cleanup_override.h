#pragma once

#include <windows.h>
#include <mmsystem.h>

namespace Platform::Testing
{
BOOL WINAPI detachContext(HDC surface, HGLRC context);
BOOL WINAPI deleteContext(HGLRC context);
int WINAPI releaseSurface(HWND window, HDC surface);
BOOL WINAPI destroyWindow(HWND window);
BOOL WINAPI unregisterWindowClass(LPCWSTR name, HINSTANCE instance);
FARPROC WINAPI lookupCleanupFunction(HMODULE module, LPCSTR name);
MMRESULT WINAPI endTimerPeriod(UINT period);
}

#define wglMakeCurrent Platform::Testing::detachContext
#define wglDeleteContext Platform::Testing::deleteContext
#define ReleaseDC Platform::Testing::releaseSurface
#define DestroyWindow Platform::Testing::destroyWindow
#define UnregisterClassW Platform::Testing::unregisterWindowClass
#define GetProcAddress Platform::Testing::lookupCleanupFunction
#define timeEndPeriod Platform::Testing::endTimerPeriod
