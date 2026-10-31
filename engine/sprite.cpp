#include "sprite.h"

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
}
