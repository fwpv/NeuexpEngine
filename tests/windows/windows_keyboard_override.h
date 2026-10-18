#pragma once

#include "keyboard_input.h"

// Подключается только при компиляции платформы для оконного теста.
#define GetKeyboardState Platform::Testing::readKeyboardState
