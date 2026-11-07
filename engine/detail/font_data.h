#pragma once
#include "font.h"
#include <array>
#include <map>

namespace Engine::Detail
{
struct FontData
{
    Texture texture;
    std::string image;
    float lineHeight = 0;
    std::uint32_t fallback = 0;
    std::map<std::uint32_t, FontGlyph> glyphs;
};
struct TextData
{
    Texture texture;
    Size2 size;
    Rect inkBounds;
    std::size_t lines = 0;
    std::vector<PositionedGlyph> glyphs;
    std::array<std::vector<TextVertex>, 2> vertices;
};
bool parseFont(const std::string& description, FontData& result, ResourceError& error);
void prepareTextGeometry(TextData& text);
}
