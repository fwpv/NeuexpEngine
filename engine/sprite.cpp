#include "sprite.h"
#include "geometry.h"
#include "math2d.h"
#include "texture.h"
#include <cmath>

namespace Engine
{
Sprite::Sprite(const Texture& texture)
    : texture(texture)
    , size(static_cast<float>(texture.width()), static_cast<float>(texture.height()))
{
}

Sprite::Sprite(const Texture& texture, const Rect& textureRect)
    : texture(texture)
    , textureRect(textureRect)
    , size(textureRect.size)
{
}

Quad Sprite::polygon() const
{
    return transformed(Rect{Vector2{}, size}.corners(), transform.toAffine());
}

bool Sprite::hitTest(Vector2 point, bool precise, float alphaThreshold) const
{
    if (!visible || !texture.isValid() || !std::isfinite(color.alpha) || color.alpha <= 0
        || color.alpha > 1 || !std::isfinite(alphaThreshold) || alphaThreshold < 0 || alphaThreshold > 1
        || !std::isfinite(size.width) || !std::isfinite(size.height) || size.width <= 0 || size.height <= 0) {
        return false;
    }
    const Rect area = textureRect.value_or(Rect{0, 0, static_cast<float>(texture.width()), static_cast<float>(texture.height())});
    if (!std::isfinite(area.position.x) || !std::isfinite(area.position.y)
        || !std::isfinite(area.size.width) || !std::isfinite(area.size.height)
        || area.position.x < 0 || area.position.y < 0 || area.size.width <= 0 || area.size.height <= 0
        || area.position.x + area.size.width > static_cast<float>(texture.width()) || area.position.y + area.size.height > static_cast<float>(texture.height())) {
        return false;
    }
    Affine2 inverse;
    if (!transform.toAffine().inverse(inverse)) {
        return false;
    }
    const Vector2 local = inverse.transformPoint(point);
    if (!(local.x >= 0 && local.y >= 0 && local.x < size.width && local.y < size.height)) {
        return false;
    }
    if (!precise) {
        return true;
    }
    const Image* pixels = texture.image();
    if (!pixels) {
        return false;
    }
    const int x = static_cast<int>(area.position.x + local.x / size.width * area.size.width);
    const int y = static_cast<int>(area.position.y + local.y / size.height * area.size.height);
    if (x >= pixels->width || y >= pixels->height) {
        return false;
    }
    const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(pixels->width)
        + static_cast<std::size_t>(x)) * 4 + 3;
    return static_cast<float>(pixels->pixels[offset]) / 255 * color.alpha > alphaThreshold;
}
}
