#include "sprite.h"
#include "geometry.h"
#include "math2d.h"
#include "texture.h"

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
}
