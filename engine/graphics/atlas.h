#pragma once

#include "sprite.h"
#include "core/geometry.h"
#include "core/resources.h"
#include "core/texture.h"

#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace NeuexpEngine
{
class TextureAtlas
{
public:
    // Строка: имя x y ширина высота (целые пиксели). Пустые строки и # комментарии разрешены.
    // Имена уникальны, без пробелов. При отказе прежний атлас сохраняется.
    bool load(const Texture& texture, const std::string& description, ResourceError& error);
    bool load(const Texture& texture, const ResourceFiles& files, const std::string& name, ResourceError& error);
    const Rect* find(std::string_view name) const;
    bool createSprite(std::string_view name, Sprite& result) const;
    // Текстура атласа; недействительна до успешной загрузки.
    const Texture& texture() const { return storedTexture; }

private:
    Texture storedTexture;
    std::map<std::string, Rect, std::less<>> regions;
};
}
