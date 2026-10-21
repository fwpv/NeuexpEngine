#pragma once

#include <cstring>

namespace Platform::Detail
{
inline bool hasExtension(const char* extensions, const char* name)
{
    if (!extensions) {
        return false;
    }
    const std::size_t length = std::strlen(name);
    const char* match = std::strstr(extensions, name);
    while (match) {
        if ((match == extensions || match[-1] == ' ') &&
            (match[length] == '\0' || match[length] == ' ')) {
            return true;
        }
        match = std::strstr(match + length, name);
    }
    return false;
}
}
