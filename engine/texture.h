#pragma once

#include "resources.h"
#include "math2d.h"

#include <memory>
#include <cstddef>

namespace Engine
{
namespace Detail
{
struct TextureData;
}

// Общая ссылка на ресурс одного рендерера. Копирование не копирует GPU-данные.
// shutdown() рендерера делает все его ссылки недействительными; их уничтожение
// после закрытия окна безопасно и не обращается к OpenGL.
class Texture
{
public:
    bool isValid() const;
    int width() const;
    int height() const;
    // nullptr, если при загрузке не запрашивалось сохранение исходных пикселей.
    // Указатель действителен, пока жива эта ссылка и не вызван shutdown().
    const Image* image() const;
    bool operator==(const Texture& other) const;
    bool operator!=(const Texture& other) const;

private:
    std::shared_ptr<Detail::TextureData> data;
    friend class Renderer2D;
};

enum class TextureFilter
{
    Nearest,
    Linear
};
enum class TextureWrap
{
    Clamp,
    Repeat
};

struct TextureSampling
{
    TextureFilter filter = TextureFilter::Linear;
    TextureWrap wrapX = TextureWrap::Clamp;
    TextureWrap wrapY = TextureWrap::Clamp;
    // Число повторений целого изображения по осям; конечные положительные значения.
    Vector2 repeats = {1, 1};
};

struct TextureStats
{
    std::size_t count = 0;
    std::size_t gpuBytes = 0;
    std::size_t retainedPixelBytes = 0;
};
}
