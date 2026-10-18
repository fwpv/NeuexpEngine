#pragma once

#include "keyboard_input.h"

namespace Platform::Testing
{
// Подмена обмена буферов с подсчётом обращений к настоящему SwapBuffers.
BOOL swapWindowBuffers(HDC deviceContext);
}

// Подключается только при компиляции платформы для оконного теста.
#define GetKeyboardState Platform::Testing::readKeyboardState
#define SwapBuffers Platform::Testing::swapWindowBuffers
