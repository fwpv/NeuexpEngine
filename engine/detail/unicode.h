#pragma once

#include <cstdint>
#include <string>

namespace Engine::Detail
{
// Кодовая точка Unicode -> UTF-8. Некорректный scalar заменяется U+FFFD.
inline void appendUtf8(std::string& text, std::uint32_t code)
{
    if (code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) {
        code = 0xfffd;
    }
    if (code < 0x80) {
        text += static_cast<char>(code);
    } else if (code < 0x800) {
        text += static_cast<char>(0xc0 | (code >> 6));
        text += static_cast<char>(0x80 | (code & 0x3f));
    } else if (code < 0x10000) {
        text += static_cast<char>(0xe0 | (code >> 12));
        text += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
        text += static_cast<char>(0x80 | (code & 0x3f));
    } else {
        text += static_cast<char>(0xf0 | (code >> 18));
        text += static_cast<char>(0x80 | ((code >> 12) & 0x3f));
        text += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
        text += static_cast<char>(0x80 | (code & 0x3f));
    }
}
}
