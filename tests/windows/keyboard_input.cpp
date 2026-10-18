#include "keyboard_input.h"

#include <algorithm>
#include <array>

namespace
{
unsigned int queryCount = 0;
bool simulatedInput = false;
std::array<BYTE, 256> keyState = {};
}

namespace Platform::Testing
{
BOOL readKeyboardState(PBYTE keys)
{
    ++queryCount;
    if (simulatedInput) {
        std::copy(keyState.begin(), keyState.end(), keys);
        return TRUE;
    }
    return GetKeyboardState(keys);
}
}

unsigned int keyboardQueryCount()
{
    return queryCount;
}

void setKeyboardKeyHeld(unsigned char keyCode, bool held)
{
    simulatedInput = true;
    keyState[keyCode] = held ? 0x80 : 0;
}

void useNativeKeyboardInput()
{
    simulatedInput = false;
    keyState = {};
}
