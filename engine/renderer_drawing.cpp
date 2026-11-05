#include "renderer.h"
#include "detail/debug_font.h"
#include "camera.h"
#include "geometry.h"
#include "math2d.h"
#include "transform.h"
#include "ui.h"

#include <cmath>
#include <string_view>

namespace Engine
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
    if (!std::isfinite(center.x) || !std::isfinite(center.y)
        || !std::isfinite(radii.x) || !std::isfinite(radii.y) || radii.x < 0 || radii.y < 0 || segments < 3) {
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

void Renderer2D::drawDebugText(std::string_view message, Vector2 position, const DrawOptions& options, float pixel)
{
    if (!ready()) {
        return;
    }
    if (!std::isfinite(pixel) || pixel <= 0 || !std::isfinite(position.x) || !std::isfinite(position.y)) {
        firstError = RenderError::InvalidGeometry;
        return;
    }
    if (!prepareDraw(options)) {
        return;
    }
    for (char character : message) {
        const unsigned int glyph = Detail::debugGlyph(character);
        for (int row = 0; row < 5; ++row) {
            for (int column = 0; column < 3; ++column) {
                const unsigned int bit = static_cast<unsigned int>((4 - row) * 3 + (2 - column));
                if ((glyph & (1u << bit)) != 0) {
                    drawRectangle(Rect{position.x + static_cast<float>(column) * pixel,
                        position.y + static_cast<float>(row) * pixel, pixel, pixel}, options);
                    if (firstError != RenderError::None) {
                        return;
                    }
                }
            }
        }
        position.x += 4 * pixel;
    }
}

void Renderer2D::drawDebugText(std::string_view message, Vector2 position, Color color, float pixel)
{
    drawDebugText(message, position, colored(color), pixel);
}
}
