#pragma once

#include "resources.h"

#include <cstdint>
#include <vector>

namespace NeuexpEngine
{
// Плотные строки RGBA8 сверху вниз; RGB не умножен на альфа-канал.
struct Image
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

// При отказе выходные значения сохраняются; error заменяется каждым вызовом.
bool validateImage(const Image& image, ResourceError& error, const ResourceLimits& limits = {});
// Декодирование через stb_image; отдельная проверка CRC контейнера не выполняется.
// Ошибки выделения памяти стандартной библиотекой передаются как исключения.
bool decodePng(const std::vector<std::uint8_t>& bytes, Image& result,
    ResourceError& error, const ResourceLimits& limits = {});
}
