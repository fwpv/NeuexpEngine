#pragma once

#include <windows.h>

namespace Platform::Testing
{
// Тестовая подмена вызова WGL.
BOOL makeContextCurrent(HDC deviceContext, HGLRC context);
BOOL deleteContext(HGLRC context);
int describePixelFormat(HDC deviceContext, int format, UINT size, LPPIXELFORMATDESCRIPTOR description);
}
