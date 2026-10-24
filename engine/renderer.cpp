#include "renderer.h"
#include "detail/polygon.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace Engine
{
namespace
{
bool finite(Vector2 point)
{
    return std::isfinite(point.x) && std::isfinite(point.y);
}

bool finite(const Affine2& transform)
{
    return std::isfinite(transform.m00) && std::isfinite(transform.m01) && std::isfinite(transform.m02)
        && std::isfinite(transform.m10) && std::isfinite(transform.m11) && std::isfinite(transform.m12);
}

bool valid(Color color)
{
    const auto component = [](float value) { return std::isfinite(value) && value >= 0 && value <= 1; };
    return component(color.red) && component(color.green) && component(color.blue) && component(color.alpha);
}

bool valid(ShapeFill fill)
{
    return fill == ShapeFill::Outline || fill == ShapeFill::Filled;
}

void vertex(Vector2 point, const Affine2& transform)
{
    const Vector2 screen = transform.transformPoint(point);
    glVertex2f(screen.x, screen.y);
}

// Каждый проход получает одинаковое начальное состояние фиксированного конвейера.
void configureState()
{
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_TEXTURE_1D);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_LINE_STIPPLE);
    glDisable(GL_POLYGON_STIPPLE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glDisable(GL_POLYGON_OFFSET_POINT);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_POINT_SMOOTH);
    glDisable(GL_POLYGON_SMOOTH);
    glDisable(GL_DITHER);
    for (int index = 0; index < 6; ++index) {
        glDisable(GL_CLIP_PLANE0 + static_cast<GLenum>(index));
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
}

bool Renderer2D::beginFrame(int width, int height, Color clearColor, RenderError& error)
{
    if (stage != Stage::Idle) {
        error = RenderError::InvalidState;
        return false;
    }
    if (width <= 0 || height <= 0) {
        error = RenderError::InvalidFrameSize;
        return false;
    }
    if (!valid(clearColor)) {
        error = RenderError::InvalidOptions;
        return false;
    }
    frameWidth = width;
    frameHeight = height;
    stage = Stage::Frame;
    configureState();
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(clearColor.red, clearColor.green, clearColor.blue, clearColor.alpha);
    glClear(GL_COLOR_BUFFER_BIT);
    error = RenderError::None;
    return true;
}

bool Renderer2D::beginView(const Viewport& area, const Affine2& transform, Stage next, RenderError& error)
{
    if (stage == Stage::Idle || (stage == Stage::Ui && next == Stage::World)) {
        error = RenderError::InvalidState;
        return false;
    }
    const std::int64_t viewportY = static_cast<std::int64_t>(frameHeight) - area.y - area.height;
    if (area.width <= 0 || area.height <= 0 || !finite(transform)
        || viewportY < std::numeric_limits<int>::min() || viewportY > std::numeric_limits<int>::max()) {
        error = RenderError::InvalidView;
        return false;
    }
    viewport = area;
    viewTransform = transform;
    clip.reset();
    stage = next;
    configureState();
    glViewport(area.x, static_cast<GLint>(viewportY), area.width, area.height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(area.x, static_cast<double>(area.x) + area.width,
        static_cast<double>(area.y) + area.height, area.y, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    applyClip();
    error = RenderError::None;
    return true;
}

bool Renderer2D::beginWorld(const CameraView& view, RenderError& error)
{
    return beginView(view.viewport, view.worldToScreenTransform, Stage::World, error);
}

bool Renderer2D::beginUi(const UiView& view, RenderError& error)
{
    return beginView(view.viewport, view.uiToScreenTransform, Stage::Ui, error);
}

void Renderer2D::applyClip() const
{
    using Wide = std::int64_t;
    Wide left = std::max<Wide>(0, viewport.x);
    Wide top = std::max<Wide>(0, viewport.y);
    Wide right = std::min<Wide>(frameWidth, static_cast<Wide>(viewport.x) + viewport.width);
    Wide bottom = std::min<Wide>(frameHeight, static_cast<Wide>(viewport.y) + viewport.height);
    if (clip) {
        left = std::max<Wide>(left, clip->x);
        top = std::max<Wide>(top, clip->y);
        right = std::min<Wide>(right, static_cast<Wide>(clip->x) + clip->width);
        bottom = std::min<Wide>(bottom, static_cast<Wide>(clip->y) + clip->height);
    }
    glEnable(GL_SCISSOR_TEST);
    if (right <= left || bottom <= top) {
        glScissor(0, 0, 0, 0);
    } else {
        glScissor(static_cast<GLint>(left), static_cast<GLint>(frameHeight - bottom),
            static_cast<GLsizei>(right - left), static_cast<GLsizei>(bottom - top));
    }
}

bool Renderer2D::setClip(const std::optional<Viewport>& area, RenderError& error)
{
    if (stage != Stage::World && stage != Stage::Ui) {
        error = RenderError::InvalidState;
        return false;
    }
    if (area && (area->width < 0 || area->height < 0)) {
        error = RenderError::InvalidClip;
        return false;
    }
    clip = area;
    applyClip();
    error = RenderError::None;
    return true;
}

bool Renderer2D::endFrame(RenderError& error)
{
    if (stage == Stage::Idle) {
        error = RenderError::InvalidState;
        return false;
    }
    glDisable(GL_SCISSOR_TEST);
    stage = Stage::Idle;
    clip.reset();
    error = RenderError::None;
    return true;
}

bool Renderer2D::prepareDraw(const DrawOptions& options, RenderError& error) const
{
    if (stage != Stage::World && stage != Stage::Ui) {
        error = RenderError::InvalidState;
        return false;
    }
    if (!valid(options.color) || !finite(options.transform)
        || !std::isfinite(options.lineWidth) || options.lineWidth <= 0
        || !std::isfinite(options.pointSize) || options.pointSize <= 0) {
        error = RenderError::InvalidOptions;
        return false;
    }
    glColor4f(options.color.red, options.color.green, options.color.blue, options.color.alpha);
    glLineWidth(options.lineWidth);
    glPointSize(options.pointSize);
    error = RenderError::None;
    return true;
}

bool Renderer2D::drawPoint(Vector2 point, const DrawOptions& options, RenderError& error)
{
    if (!finite(point)) {
        error = RenderError::InvalidGeometry;
        return false;
    }
    if (!prepareDraw(options, error)) {
        return false;
    }
    glBegin(GL_POINTS);
    vertex(point, viewTransform * options.transform);
    glEnd();
    return true;
}

bool Renderer2D::drawLine(Vector2 start, Vector2 end, const DrawOptions& options, RenderError& error)
{
    if (!finite(start) || !finite(end)) {
        error = RenderError::InvalidGeometry;
        return false;
    }
    if (!prepareDraw(options, error)) {
        return false;
    }
    const Affine2 transform = viewTransform * options.transform;
    glBegin(GL_LINES);
    vertex(start, transform);
    vertex(end, transform);
    glEnd();
    return true;
}

bool Renderer2D::drawRectangle(const Rect& rectangle, ShapeFill fill, const DrawOptions& options, RenderError& error)
{
    if (!finite(rectangle.position) || !std::isfinite(rectangle.size.width) || !std::isfinite(rectangle.size.height)
        || rectangle.size.width < 0 || rectangle.size.height < 0 || !valid(fill)) {
        error = RenderError::InvalidGeometry;
        return false;
    }
    if (!prepareDraw(options, error)) {
        return false;
    }
    const Affine2 transform = viewTransform * options.transform;
    glBegin(fill == ShapeFill::Filled ? GL_QUADS : GL_LINE_LOOP);
    vertex(rectangle.position, transform);
    vertex(rectangle.position + Vector2{ rectangle.size.width, 0 }, transform);
    vertex(rectangle.position + Vector2{ rectangle.size.width, rectangle.size.height }, transform);
    vertex(rectangle.position + Vector2{ 0, rectangle.size.height }, transform);
    glEnd();
    return true;
}

bool Renderer2D::drawCircle(const Circle& circle, ShapeFill fill, const DrawOptions& options,
    RenderError& error, unsigned int segments)
{
    if (!finite(circle.center) || !std::isfinite(circle.radius) || circle.radius < 0 || segments < 3 || !valid(fill)) {
        error = RenderError::InvalidGeometry;
        return false;
    }
    if (!prepareDraw(options, error)) {
        return false;
    }
    if (circle.radius == 0) {
        return true;
    }
    const Affine2 transform = viewTransform * options.transform;
    glBegin(fill == ShapeFill::Filled ? GL_TRIANGLE_FAN : GL_LINE_LOOP);
    if (fill == ShapeFill::Filled) {
        vertex(circle.center, transform);
    }
    for (unsigned int index = 0; index < segments; ++index) {
        const float angle = 2 * Pi * (static_cast<float>(index) / static_cast<float>(segments));
        vertex(circle.center + Vector2{ std::cos(angle), std::sin(angle) } * circle.radius, transform);
    }
    if (fill == ShapeFill::Filled) {
        vertex(circle.center + Vector2{ circle.radius, 0 }, transform);
    }
    glEnd();
    return true;
}

bool Renderer2D::drawPolygon(const std::vector<Vector2>& vertices, ShapeFill fill,
    const DrawOptions& options, RenderError& error)
{
    if (!valid(fill)) {
        error = RenderError::InvalidGeometry;
        return false;
    }
    // Подготовка геометрии завершается до отправки команд: отказ не оставляет частичной заливки.
    std::vector<Vector2> contour;
    std::vector<std::array<std::size_t, 3>> triangles;
    if (!Detail::triangulatePolygon(vertices, contour, triangles)) {
        error = RenderError::InvalidGeometry;
        return false;
    }
    if (!prepareDraw(options, error)) {
        return false;
    }
    const Affine2 transform = viewTransform * options.transform;
    if (fill == ShapeFill::Filled) {
        glBegin(GL_TRIANGLES);
        for (const auto& triangle : triangles) {
            for (std::size_t index : triangle) {
                vertex(contour[index], transform);
            }
        }
    } else {
        glBegin(GL_LINE_LOOP);
        for (Vector2 point : contour) {
            vertex(point, transform);
        }
    }
    glEnd();
    return true;
}
}
