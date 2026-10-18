#pragma once

#include "windows_gl_context.h"

// Заголовки Windows уже подключены, поэтому макрос не меняет объявление системной функции.
// Этот файл принудительно подключается только при компиляции тестовой копии платформы.
#define wglMakeCurrent Platform::Testing::makeContextCurrent
