#pragma once

#include <windows.h>

// Счётчик чтений клавиатуры в тестовой копии платформы.
unsigned int keyboardQueryCount();
void setKeyboardKeyHeld(unsigned char keyCode, bool held);
void useNativeKeyboardInput();

namespace Platform::Testing
{
// Подмена GetKeyboardState; состояние рабочего стола не изменяется.
BOOL readKeyboardState(PBYTE keys);
}
