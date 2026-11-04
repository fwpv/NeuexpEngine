#pragma once

#include "sprite.h"
#include <map>
#include <string>

namespace Engine
{
class TextureAtlas
{
public:
    // Строка: имя x y ширина высота (целые пиксели). Пустые строки и # комментарии разрешены.
    // Имена уникальны, без пробелов. При отказе прежний атлас сохраняется.
    bool load(const Texture& texture, const std::string& description, ResourceError& error);
    bool load(const Texture& texture, const ResourceFiles& files, const std::string& name, ResourceError& error);
    const Rect* find(const std::string& name) const;
    bool createSprite(const std::string& name, Sprite& result) const;

private:
    Texture texture;
    std::map<std::string, Rect> regions;
};
}
