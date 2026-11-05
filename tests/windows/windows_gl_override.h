#pragma once

#include "windows_gl_context.h"

// Принудительное подключение после Windows.h только для тестовой копии платформы.
#define wglMakeCurrent Platform::Testing::makeContextCurrent
#define wglDeleteContext Platform::Testing::deleteContext
#define DescribePixelFormat Platform::Testing::describePixelFormat
