#include <algorithm>
#include "detail/unicode.h"
#include "renderer.h"
#include "detail/debug_font.h"
#include "core/camera.h"
#include "core/geometry.h"
#include "core/math2d.h"
#include "core/transform.h"
#include "ui_view.h"

#include <cmath>
#include <string_view>

namespace NeuexpEngine
{
namespace
{
DrawOptions colored(Color color)
{
    DrawOptions options;
    options.color = color;
    return options;
}
}

const char* renderErrorMessage(RenderError error)
{
    switch (error) {
        case RenderError::None: return "No rendering error.";
        case RenderError::InvalidState: return "The operation is not allowed in the current renderer state.";
        case RenderError::InvalidFrameSize: return "Frame dimensions must be positive.";
        case RenderError::InvalidView: return "The view has an invalid viewport or transform.";
        case RenderError::InvalidClip: return "Clipping dimensions must be nonnegative.";
        case RenderError::InvalidGeometry: return "The drawing geometry is invalid.";
        case RenderError::InvalidOptions: return "The drawing options are invalid.";
        case RenderError::NoCurrentContext: return "No current OpenGL context is available.";
        case RenderError::InvalidTexture: return "The texture is invalid or belongs to another renderer.";
        case RenderError::Graphics: return "OpenGL reported an error while rendering the frame.";
    }
    return "Unknown rendering error.";
}

bool Renderer2D::ready()
{
    if (firstError != RenderError::None) {
        return false;
    }
    if (stage == Stage::Idle) {
        firstError = RenderError::InvalidState;
        return false;
    }
    return true;
}

void Renderer2D::setWorldView(const CameraView& view)
{
    if (ready()) {
        selectView(view.viewport, view.worldToScreenTransform, Stage::World);
    }
}

void Renderer2D::setUiView(const UiView& view)
{
    if (ready()) {
        selectView(view.viewport, view.uiToScreenTransform, Stage::Ui);
    }
}

void Renderer2D::drawPoint(Vector2 point, Color color)
{
    drawPoint(point, colored(color));
}

void Renderer2D::drawLine(Vector2 start, Vector2 end, Color color)
{
    drawLine(start, end, colored(color));
}

void Renderer2D::drawRectangle(const Rect& bounds, const DrawOptions& options, ShapeFill fill)
{
    if (ready()) {
        queueRectangle(bounds, fill, options);
    }
}

void Renderer2D::drawRectangle(const Rect& bounds, Color color, ShapeFill fill)
{
    drawRectangle(bounds, colored(color), fill);
}

void Renderer2D::drawCircle(const Circle& circle, Color color, ShapeFill fill, unsigned int segments)
{
    drawCircle(circle, colored(color), fill, segments);
}

void Renderer2D::drawEllipse(Vector2 center, Vector2 radii, const DrawOptions& options,
    ShapeFill fill, unsigned int segments)
{
    if (!ready()) {
        return;
    }
    if (radii.x < 0 || radii.y < 0 || segments < 3) {
        firstError = RenderError::InvalidGeometry;
        return;
    }
    DrawOptions transformed = options;
    transformed.transform = options.transform * Transform2{center, 0, radii, Vector2{}}.toAffine();
    drawCircle(Circle{Vector2{}, 1}, transformed, fill, segments);
}

void Renderer2D::drawEllipse(Vector2 center, Vector2 radii, Color color, ShapeFill fill, unsigned int segments)
{
    drawEllipse(center, radii, colored(color), fill, segments);
}

void Renderer2D::drawPolygon(const Polygon& polygon, const DrawOptions& options, ShapeFill fill)
{
    queuePolygon(polygon.data(), polygon.size(), options, fill);
}

void Renderer2D::drawPolygon(const Polygon& polygon, Color color, ShapeFill fill)
{
    drawPolygon(polygon, colored(color), fill);
}

void Renderer2D::drawPolygon(const Quad& polygon, const DrawOptions& options, ShapeFill fill)
{
    queuePolygon(polygon.data(), polygon.size(), options, fill);
}

void Renderer2D::drawPolygon(const Quad& polygon, Color color, ShapeFill fill)
{
    drawPolygon(polygon, colored(color), fill);
}

namespace
{
// Ширина строки без завершающего межбуквенного интервала — как у measureDebugText.
float debugLineWidth(std::string_view line, float pixel)
{
    float x = 0;
    float width = 0;
    for (std::size_t offset = 0; offset < line.size();) {
        std::uint32_t code;
        Detail::nextUtf8(line, offset, code);
        const auto glyph = Detail::unicodeDebugGlyph(code);
        width = std::max(width, x + static_cast<float>(glyph.width) * pixel);
        x += static_cast<float>(glyph.width + 1) * pixel;
    }
    return width;
}

// Перенос по словам: пробелы на границе переноса не сохраняются, слова не делятся.
std::vector<std::string_view> wrapDebugLine(std::string_view line, float pixel, float wrapWidth)
{
    std::vector<std::string_view> lines;
    if (wrapWidth <= 0 || line.empty()) {
        lines.push_back(line);
        return lines;
    }
    std::size_t lineBegin = 0;
    std::size_t lineEnd = 0;
    std::size_t begin = 0;
    bool hasWord = false;
    for (std::size_t offset = 0; offset <= line.size();) {
        if (offset == line.size() || line[offset] == ' ') {
            const std::string_view word = line.substr(begin, offset - begin);
            const std::size_t wordBegin = begin;
            begin = offset + 1;
            if (!word.empty()) {
                if (!hasWord) {
                    lineBegin = wordBegin;
                    lineEnd = offset;
                    hasWord = true;
                } else if (debugLineWidth(line.substr(lineBegin, offset - lineBegin), pixel) <= wrapWidth) {
                    lineEnd = offset;
                } else {
                    lines.push_back(line.substr(lineBegin, lineEnd - lineBegin));
                    lineBegin = wordBegin;
                    lineEnd = offset;
                }
            }
        }
        ++offset;
    }
    lines.push_back(hasWord ? line.substr(lineBegin, lineEnd - lineBegin) : std::string_view{});
    return lines;
}
}

void Renderer2D::drawDebugText(std::string_view message, Vector2 position, const DrawOptions& options,
    float pixel, TextAlignment alignment, float wrapWidth)
{
    if (!ready()) {
        return;
    }
    if (!(pixel > 0) || !std::isfinite(pixel)) {
        firstError = RenderError::InvalidGeometry;
        return;
    }
    if (!(wrapWidth >= 0) || !std::isfinite(wrapWidth)) {
        firstError = RenderError::InvalidGeometry;
        return;
    }
    if (!prepareDraw(options)) {
        return;
    }
    const float lineHeight = 8 * pixel;
    std::size_t lineIndex = 0;
    for (std::size_t begin = 0; begin <= message.size();) {
        const std::size_t found = message.find('\n', begin);
        const std::string_view source = message.substr(begin,
            found == std::string_view::npos ? message.size() - begin : found - begin);
        for (std::string_view line : wrapDebugLine(source, pixel, wrapWidth)) {
            const float width = debugLineWidth(line, pixel);
            float offset = 0;
            if (alignment == TextAlignment::Center) {
                offset = -width / 2;
            } else if (alignment == TextAlignment::Right) {
                offset = -width;
            }
            const float startX = position.x + std::round(offset);
            const float startY = position.y + static_cast<float>(lineIndex) * lineHeight;
            float cursor = startX;
            for (std::size_t offsetInLine = 0; offsetInLine < line.size();) {
                std::uint32_t code;
                Detail::nextUtf8(line, offsetInLine, code);
                const auto glyph = Detail::unicodeDebugGlyph(code);
                for (int row = 0; row < glyph.height; ++row) {
                    for (int column = 0; column < glyph.width; ++column) {
                        const auto bit = static_cast<unsigned int>((glyph.height - 1 - row) * glyph.width
                            + glyph.width - 1 - column);
                        if ((glyph.bits & (std::uint64_t{1} << bit)) != 0) {
                            drawRectangle(Rect{cursor + static_cast<float>(column) * pixel,
                                startY + static_cast<float>(row) * pixel, pixel, pixel}, options);
                            if (firstError != RenderError::None) {
                                return;
                            }
                        }
                    }
                }
                cursor += static_cast<float>(glyph.width + 1) * pixel;
            }
            ++lineIndex;
        }
        if (found == std::string_view::npos) {
            break;
        }
        begin = found + 1;
    }
}

void Renderer2D::drawDebugText(std::string_view message, Vector2 position, Color color, float pixel,
    TextAlignment alignment, float wrapWidth)
{
    drawDebugText(message, position, colored(color), pixel, alignment, wrapWidth);
}
}

namespace NeuexpEngine
{
Size2 Renderer2D::measureDebugText(std::string_view message, float pixel)
{
    Size2 result;
    float x = 0;
    float y = 0;
    for (std::size_t offset = 0; offset < message.size();) {
        std::uint32_t code;
        Detail::nextUtf8(message, offset, code);
        if (code == '\n') {
            x = 0;
            y += 8 * pixel;
            result.height = std::max(result.height, y);
            continue;
        }
        const auto glyph = Detail::unicodeDebugGlyph(code);
        result.width = std::max(result.width, x + static_cast<float>(glyph.width) * pixel);
        result.height = std::max(result.height, y + static_cast<float>(glyph.height) * pixel);
        x += static_cast<float>(glyph.width + 1) * pixel;
    }
    return result;
}
}
