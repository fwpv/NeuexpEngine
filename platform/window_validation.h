#pragma once

#include "platform.h"

namespace Platform
{
// Размеры X11 передаются в 16-битных беззнаковых полях протокола.
constexpr int MaximumWindowDimension = 65535;

inline bool validateWindowOptions(const WindowOptions& options, Error& error)
{
    if (options.width <= 0 || options.height <= 0
        || options.width > MaximumWindowDimension || options.height > MaximumWindowDimension
        || options.minimumWidth < 0 || options.minimumHeight < 0
        || options.minimumWidth > options.width || options.minimumHeight > options.height) {
        error = { "initialize", "Invalid window dimensions: initial sizes must be in [1, 65535], minimum sizes in [0, initial size].", std::nullopt };
        return false;
    }
    return true;
}
}
