#include "window_validation.h"

#include <limits>

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
        const auto first = static_cast<unsigned char>(title[index++]);
        if (first == 0) {
            valid = false;
        } else if (first < 0x80) {
            continue;
        } else {
            // Первый байт задаёт число продолжений; каждый следующий байт имеет вид 10xxxxxx.
            int continuationCount = 0;
            unsigned int codePoint = 0;
            unsigned int minimumCodePoint = 0;
            if (first >= 0xC2 && first <= 0xDF) {
                continuationCount = 1;
                codePoint = first & 0x1FU;
                minimumCodePoint = 0x80;
            } else if (first >= 0xE0 && first <= 0xEF) {
                continuationCount = 2;
                codePoint = first & 0x0FU;
                minimumCodePoint = 0x800;
            } else if (first >= 0xF0 && first <= 0xF4) {
                continuationCount = 3;
                codePoint = first & 0x07U;
                minimumCodePoint = 0x10000;
            } else {
                valid = false;
            }
            for (int part = 0; valid && part < continuationCount; ++part) {
                if (index == title.size()) {
                    valid = false;
                    break;
                }
                const auto byte = static_cast<unsigned char>(title[index++]);
                valid = (byte & 0xC0U) == 0x80U;
                codePoint = (codePoint << 6U) | (byte & 0x3FU);
            }
            // Избыточные кодировки, суррогаты UTF-16 и значения вне Unicode недопустимы.
            valid = valid && codePoint >= minimumCodePoint && codePoint <= 0x10FFFF
                && !(codePoint >= 0xD800 && codePoint <= 0xDFFF);
        }
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
