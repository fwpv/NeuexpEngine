#pragma once

#include <windows.h>

namespace Platform::Testing
{
// Тестовая подмена вызова WGL; реализация находится в platform_startup_rollback_test.cpp.
BOOL makeContextCurrent(HDC deviceContext, HGLRC context);
BOOL deleteContext(HGLRC context);
int describePixelFormat(HDC deviceContext, int format, UINT size, LPPIXELFORMATDESCRIPTOR description);
}
