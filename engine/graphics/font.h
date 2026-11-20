#pragma once

#include "core/geometry.h"
#include "core/resources.h"
#include "core/texture.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace NeuexpEngine
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
// Все числовые параметры конечны по контракту вызывающего кода.
struct TextLayoutOptions
{
    // Ноль отключает перенос. Ширина задаётся в итоговых локальных единицах.
    float width = 0;
    float scale = 1;
    // Дополнительный интервал в пикселях шрифта, масштабируется вместе с ним.
    float lineSpacing = 0;
    TextAlignment alignment = TextAlignment::Left;
    // Округляет горизонтальное смещение всей строки в локальных единицах.
    // Пиксельная чёткость требует масштаба 1:1 и целой экранной позиции.
    bool roundAlignment = true;
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

// Подготовленная вершина: положение в тексте и координата в пикселях исходного атласа.
struct TextVertex
{
    Vector2 position;
    Vector2 texel;
};

class BitmapFont;

class TextLayout
{
public:
    // Перестраивает раскладку; при ошибке прежнее содержимое сохраняется.
    bool build(const BitmapFont& font, std::string_view text, ResourceError& error,
        const TextLayoutOptions& options = {});
    bool isValid() const;
    Texture texture() const;
    // Только чтение. Ссылки действительны до изменения или уничтожения раскладки.
    const std::vector<TextVertex>& vertices(TextureFilter filter) const;
    // Логический размер включает пробелы и пустые строки; пустой текст имеет размер 0x0.
    Size2 size() const;
    // Объединение прямоугольников глифов с учётом смещений; может выходить за логическую область.
    Rect inkBounds() const;
    std::size_t lineCount() const;
    const std::vector<PositionedGlyph>& glyphs() const;

private:
    std::shared_ptr<const Detail::TextData> data;
};

class BitmapFont
{
public:
    bool isValid() const;
    float lineHeight() const;
    Texture texture() const;
    std::uint32_t fallbackCodepoint() const;
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
};
}
