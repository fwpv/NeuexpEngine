#include "keyboard_query_counter.h"

#include <X11/Xlib.h>
#include <algorithm>
#include <array>

namespace
{
unsigned int queryCount = 0;
bool simulatedInput = false;
std::array<char, 32> keyMap = {};
}

extern "C" int __real_XQueryKeymap(Display* display, char keys[32]);

// --wrap подменяет ответ только в сценарии клавиатуры; остальные тесты обращаются к X11.
extern "C" int __wrap_XQueryKeymap(Display* display, char keys[32])
{
    ++queryCount;
    if (simulatedInput) {
        std::copy(keyMap.begin(), keyMap.end(), keys);
        return 1;
    }
    return __real_XQueryKeymap(display, keys);
}

unsigned int keyboardQueryCount()
{
    return queryCount;
}

void setKeyboardKeyHeld(unsigned char keyCode, bool held)
{
    simulatedInput = true;
    auto bits = static_cast<unsigned char>(keyMap[keyCode / 8]);
    const auto mask = static_cast<unsigned char>(1U << (keyCode % 8));
    if (held) {
        bits |= mask;
    } else {
        bits &= static_cast<unsigned char>(~mask);
    }
    keyMap[keyCode / 8] = static_cast<char>(bits);
}

void useNativeKeyboardInput()
{
    simulatedInput = false;
    keyMap = {};
}
