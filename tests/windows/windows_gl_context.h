#pragma once

#include <windows.h>

namespace Platform::Testing
{
// Тестовая подмена вызова WGL; реализация находится в platform_startup_rollback_test.cpp.
BOOL makeContextCurrent(HDC deviceContext, HGLRC context);
}
