#include "window_validation.h"
#include "app/platform.h"
#include "detail/unicode.h"

#include <limits>
#include <cstddef>
#include <optional>
#include <string>

namespace Platform
{
namespace
{
// Размеры X11 передаются в 16-битных беззнаковых полях протокола.
constexpr int MaximumWindowDimension = 65535;
}

// Проверка UTF-8 до передачи строки в системный API.
bool validateWindowTitle(const std::string& title, const char* operation, Error& error)
{
    bool valid = title.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max());
    for (std::size_t index = 0; valid && index < title.size();) {
        std::uint32_t code = 0;
        valid = NeuexpEngine::Detail::nextUtf8(title, index, code) && code != 0;
    }

    if (!valid) {
        error = {operation, "Window title must be valid UTF-8 without embedded null characters.", std::nullopt};
    }
    return valid;
}

bool validateWindowOptions(const WindowOptions& options, Error& error)
{
    if (options.width <= 0 || options.height <= 0
        || options.width > MaximumWindowDimension || options.height > MaximumWindowDimension
        || options.minimumWidth < 0 || options.minimumHeight < 0
        || options.minimumWidth > options.width || options.minimumHeight > options.height) {
        error = {"initialize", "Invalid window dimensions: initial sizes must be in [1, 65535], minimum sizes in [0, initial size].", std::nullopt};
        return false;
    }
    return validateWindowTitle(options.title, "initialize", error);
}
}
