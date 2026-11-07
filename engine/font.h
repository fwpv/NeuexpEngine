#pragma once

#include "geometry.h"
#include "resources.h"
#include "texture.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Engine
{
namespace Detail
{
struct FontData;
struct TextData;
}

enum class TextAlignment
{
    Left,
    Center,
    Right
};
struct TextLayoutOptions
{
    // Ноль отключает перенос. Ширина задаётся в итоговых локальных единицах.
    float width = 0;
    float scale = 1;
    // Дополнительный интервал в пикселях шрифта, масштабируется вместе с ним.
    float lineSpacing = 0;
    TextAlignment alignment = TextAlignment::Left;
};

struct FontGlyph
{
    Rect area;
    Vector2 offset;
    float advance = 0;
};

struct PositionedGlyph
{
    std::uint32_t codepoint = 0;
    Rect area;
    Rect bounds;
};

class TextLayout
{
public:
    bool isValid() const;
    // Логический размер включает пробелы и пустые строки; пустой текст имеет размер 0x0.
    Size2 size() const;
    // Объединение прямоугольников глифов с учётом смещений; может выходить за логическую область.
    Rect inkBounds() const;
    std::size_t lineCount() const;
    const std::vector<PositionedGlyph>& glyphs() const;

private:
    std::shared_ptr<const Detail::TextData> data;
    friend class BitmapFont;
    friend class Renderer2D;
};

class BitmapFont
{
public:
    bool isValid() const;
    float lineHeight() const;
    const FontGlyph* find(std::uint32_t codepoint) const;
    // Формат bitmap_font 1. Готовая текстура должна содержать белую альфа-маску.
    // Ошибка не изменяет ранее загруженный шрифт.
    bool load(const Texture& texture, const std::string& description, ResourceError& error);
    // CRLF и CR считаются переносом; tab — четыре пробела. Перенос по словам,
    // длинные слова делятся по символам. Пробелы на автоматическом переносе удаляются.
    // Ошибочный UTF-8 заменяется U+FFFD, отсутствующие глифы — заданным fallback.
    // Не более 65536 байт UTF-8 на блок. Ошибка оставляет result неизменным.
    // Раскладка владеет текстурой независимо от времени жизни BitmapFont.
    bool layout(std::string_view text, TextLayout& result, ResourceError& error,
        const TextLayoutOptions& options = {}) const;

private:
    std::shared_ptr<const Detail::FontData> data;
    friend class Renderer2D;
};
}
