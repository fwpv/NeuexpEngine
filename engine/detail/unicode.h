#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <cstddef>

namespace Engine::Detail
{
// При ошибке потребляет один байт, записывает U+FFFD и возвращает false.
inline bool nextUtf8(std::string_view text, std::size_t& offset, std::uint32_t& result)
{
    result = 0xfffd;
    const auto first = static_cast<unsigned char>(text[offset++]);
    if (first < 0x80) {
        result = first;
        return true;
    }
    const int count = first >= 0xc2 && first <= 0xdf ? 2
        : first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
    if (count == 0 || text.size() - offset < static_cast<std::size_t>(count - 1)) {
        return false;
    }
    std::uint32_t code = first & (0x7fU >> count);
    for (int index = 0; index < count - 1; ++index) {
        const auto byte = static_cast<unsigned char>(text[offset + static_cast<std::size_t>(index)]);
        if ((byte & 0xc0) != 0x80) {
            return false;
        }
        code = (code << 6) | (byte & 0x3fU);
    }
    if ((count == 2 && code < 0x80) || (count == 3 && code < 0x800)
        || (count == 4 && code < 0x10000) || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) {
        return false;
    }
    offset += static_cast<std::size_t>(count - 1);
    result = code;
    return true;
}

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
