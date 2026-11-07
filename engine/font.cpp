#include "font.h"
#include "detail/font_data.h"
#include "detail/texture_data.h"
#include "detail/unicode.h"

#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>
#include <utility>

namespace Engine
{
namespace
{
bool fail(ResourceError& error, const std::string& message)
{
    error = {ResourceErrorCode::InvalidImage, message};
    return false;
}
bool scalar(std::uint32_t code)
{
    return code <= 0x10ffff && !(code >= 0xd800 && code <= 0xdfff);
}
}

bool Detail::parseFont(const std::string& description, FontData& result, ResourceError& error)
{
    FontData parsed;
    bool header = false;
    bool height = false;
    bool fallback = false;
    std::istringstream lines(description);
    std::string line;
    std::size_t number = 0;
    while (std::getline(lines, line)) {
        ++number;
        std::istringstream fields(line.substr(0, line.find('#')));
        fields.imbue(std::locale::classic());
        std::string kind;
        if (!(fields >> kind)) {
            continue;
        }
        bool valid = true;
        if (!header) {
            int version = 0;
            valid = kind == "bitmap_font" && (fields >> version) && version == 1;
            header = valid;
        } else if (kind == "texture") {
            valid = parsed.image.empty() && static_cast<bool>(fields >> parsed.image);
        } else if (kind == "lineHeight") {
            int value = 0;
            valid = !height && (fields >> value) && value > 0;
            parsed.lineHeight = static_cast<float>(value);
            height = true;
        } else if (kind == "fallback") {
            valid = !fallback && (fields >> parsed.fallback) && scalar(parsed.fallback);
            fallback = true;
        } else if (kind == "glyph") {
            std::uint32_t code = 0;
            int x = 0;
            int y = 0;
            int width = 0;
            int glyphHeight = 0;
            int offsetX = 0;
            int offsetY = 0;
            int advance = 0;
            valid = static_cast<bool>(fields >> code >> x >> y >> width >> glyphHeight >> offsetX >> offsetY >> advance)
                && scalar(code) && x >= 0 && y >= 0 && width >= 0 && glyphHeight >= 0 && advance >= 0;
            if (valid) {
                valid = parsed.glyphs.emplace(code, FontGlyph{
                    Rect{static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(glyphHeight)},
                    Vector2{static_cast<float>(offsetX), static_cast<float>(offsetY)}, static_cast<float>(advance)}).second;
            }
        } else {
            valid = false;
        }
        std::string extra;
        if (!valid || (fields >> extra)) {
            return fail(error, "Invalid font record at line " + std::to_string(number));
        }
    }
    if (!header || !height || !fallback || parsed.image.empty() || parsed.glyphs.count(parsed.fallback) == 0
        || parsed.glyphs.count(32) == 0) {
        return fail(error, "Font requires texture, lineHeight, fallback glyph and space glyph.");
    }
    result = std::move(parsed);
    error = {};
    return true;
}

bool BitmapFont::load(const Texture& texture, const std::string& description, ResourceError& error)
{
    if (!texture.isValid()) {
        error = {ResourceErrorCode::InvalidState, "Font requires a valid texture."};
        return false;
    }
    auto parsed = std::make_shared<Detail::FontData>();
    if (!Detail::parseFont(description, *parsed, error)) {
        return false;
    }
    for (const auto& entry : parsed->glyphs) {
        const auto& area = entry.second.area;
        if (static_cast<double>(area.position.x) + area.size.width > texture.width()
            || static_cast<double>(area.position.y) + area.size.height > texture.height()) {
            return fail(error, "Glyph lies outside font texture: " + std::to_string(entry.first));
        }
    }
    parsed->texture = texture;
    data = std::move(parsed);
    error = {};
    return true;
}
bool BitmapFont::isValid() const
{
    return data && data->texture.isValid();
}
float BitmapFont::lineHeight() const
{
    return data ? data->lineHeight : 0;
}
const FontGlyph* BitmapFont::find(std::uint32_t codepoint) const
{
    if (!data) {
        return nullptr;
    }
    const auto found = data->glyphs.find(codepoint);
    return found == data->glyphs.end() ? nullptr : &found->second;
}

bool BitmapFont::layout(std::string_view text, TextLayout& result, ResourceError& error,
    const TextLayoutOptions& options) const
{
    if (!isValid()) {
        error = {ResourceErrorCode::InvalidState, "Load a font before laying out text."};
        return false;
    }
    if (!std::isfinite(options.width) || options.width < 0 || !std::isfinite(options.scale) || options.scale <= 0
        || !std::isfinite(options.lineSpacing) || options.lineSpacing < 0
        || (options.alignment != TextAlignment::Left && options.alignment != TextAlignment::Center
            && options.alignment != TextAlignment::Right)) {
        return fail(error, "Invalid text layout options.");
    }
    // Ограничивает объём двух подготовленных массивов геометрии и время размещения.
    if (text.size() > 65536) {
        error = {ResourceErrorCode::LimitExceeded, "Text exceeds 65536 UTF-8 bytes."};
        return false;
    }
    std::vector<std::uint32_t> codes;
    for (std::size_t offset = 0; offset < text.size();) {
        std::uint32_t code = 0;
        Detail::nextUtf8(text, offset, code);
        if (code == '\r') {
            if (offset < text.size() && text[offset] == '\n') {
                ++offset;
            }
            code = '\n';
        }
        if (code == '\t') {
            codes.insert(codes.end(), 4, 32);
        } else {
            codes.push_back(code);
        }
    }
    auto placed = std::make_shared<Detail::TextData>();
    placed->texture = data->texture;
    struct Line
    {
        std::size_t begin;
        std::size_t end;
        float width;
    };
    std::vector<Line> lines;
    float x = 0;
    float y = 0;
    std::size_t lineBegin = 0;
    const float step = (data->lineHeight + options.lineSpacing) * options.scale;
    const auto glyphFor = [&](std::uint32_t code) -> const FontGlyph& {
        const auto* glyph = find(code);
        return glyph ? *glyph : data->glyphs.at(data->fallback);
    };
    const auto finishLine = [&] {
        lines.push_back(Line{lineBegin, placed->glyphs.size(), x});
        lineBegin = placed->glyphs.size();
        x = 0;
        y += step;
    };
    for (std::size_t index = 0; index < codes.size();) {
        if (codes[index] == '\n') {
            finishLine();
            ++index;
            continue;
        }
        // Пробелы отделяются от слова, чтобы не оставлять их на автоматическом переносе.
        const std::size_t spacesBegin = index;
        float spaces = 0;
        while (index < codes.size() && codes[index] == 32) {
            spaces += glyphFor(32).advance * options.scale;
            ++index;
        }
        const std::size_t wordBegin = index;
        float wordWidth = 0;
        while (index < codes.size() && codes[index] != 32 && codes[index] != '\n') {
            wordWidth += glyphFor(codes[index]).advance * options.scale;
            ++index;
        }
        if (options.width > 0 && x > 0 && wordBegin < index && x + spaces + wordWidth > options.width) {
            finishLine();
        } else {
            for (std::size_t space = spacesBegin; space < wordBegin; ++space) {
                const float advance = glyphFor(32).advance * options.scale;
                if (options.width > 0 && x > 0 && x + advance > options.width) {
                    finishLine();
                }
                x += advance;
            }
        }
        for (std::size_t letter = wordBegin; letter < index; ++letter) {
            const auto code = find(codes[letter]) ? codes[letter] : data->fallback;
            const auto& glyph = glyphFor(code);
            const float advance = glyph.advance * options.scale;
            if (options.width > 0 && x > 0 && x + advance > options.width) {
                finishLine();
            }
            if (glyph.area.size.width > 0 && glyph.area.size.height > 0) {
                placed->glyphs.push_back(PositionedGlyph{code, glyph.area,
                    Rect{Vector2{x, y} + glyph.offset * options.scale,
                        Size2{glyph.area.size.width * options.scale, glyph.area.size.height * options.scale}}});
            }
            x += advance;
        }
    }
    if (!codes.empty()) {
        finishLine();
        placed->size.height = data->lineHeight * options.scale + static_cast<float>(lines.size() - 1) * step;
        placed->size.width = options.width;
        for (const auto& line : lines) {
            placed->size.width = std::max(placed->size.width, line.width);
        }
    }
    placed->lines = lines.size();
    bool ink = false;
    for (const auto& line : lines) {
        const float remaining = placed->size.width - line.width;
        const float offset = options.alignment == TextAlignment::Center ? remaining / 2
            : options.alignment == TextAlignment::Right ? remaining : 0;
        for (std::size_t index = line.begin; index < line.end; ++index) {
            auto& bounds = placed->glyphs[index].bounds;
            bounds.position.x += offset;
            if (!ink) {
                placed->inkBounds = bounds;
                ink = true;
            } else {
                const float right = std::max(placed->inkBounds.position.x + placed->inkBounds.size.width,
                    bounds.position.x + bounds.size.width);
                const float bottom = std::max(placed->inkBounds.position.y + placed->inkBounds.size.height,
                    bounds.position.y + bounds.size.height);
                placed->inkBounds.position.x = std::min(placed->inkBounds.position.x, bounds.position.x);
                placed->inkBounds.position.y = std::min(placed->inkBounds.position.y, bounds.position.y);
                placed->inkBounds.size = {right - placed->inkBounds.position.x, bottom - placed->inkBounds.position.y};
            }
        }
    }
    if (!std::isfinite(placed->size.width) || !std::isfinite(placed->size.height)
        || !std::isfinite(placed->inkBounds.position.x) || !std::isfinite(placed->inkBounds.position.y)
        || !std::isfinite(placed->inkBounds.size.width) || !std::isfinite(placed->inkBounds.size.height)) {
        return fail(error, "Text dimensions overflow.");
    }
    Detail::prepareTextGeometry(*placed, *data->texture.data);
    result.data = std::move(placed);
    error = {};
    return true;
}

bool TextLayout::isValid() const
{
    return data && data->texture.isValid();
}
Size2 TextLayout::size() const
{
    return data ? data->size : Size2{};
}
Rect TextLayout::inkBounds() const
{
    return data ? data->inkBounds : Rect{};
}
std::size_t TextLayout::lineCount() const
{
    return data ? data->lines : 0;
}
const std::vector<PositionedGlyph>& TextLayout::glyphs() const
{
    static const std::vector<PositionedGlyph> empty;
    return data ? data->glyphs : empty;
}
}
