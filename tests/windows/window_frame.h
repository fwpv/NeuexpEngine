#pragma once

#include <windows.h>

namespace PlatformTest
{
inline bool adjustWindowFrame(HWND window, RECT& bounds, DWORD style, DWORD extendedStyle = 0)
{
    using GetDpi = UINT (WINAPI*)(HWND);
    using AdjustFrame = BOOL (WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
    const HMODULE user = GetModuleHandleW(L"user32.dll");
    const auto getDpi = reinterpret_cast<GetDpi>(reinterpret_cast<void (WINAPI*)()>(GetProcAddress(user, "GetDpiForWindow")));
    const auto adjust = reinterpret_cast<AdjustFrame>(reinterpret_cast<void (WINAPI*)()>(GetProcAddress(user, "AdjustWindowRectExForDpi")));
    return getDpi && adjust && adjust(&bounds, style, FALSE, extendedStyle, getDpi(window)) != FALSE;
}
}
