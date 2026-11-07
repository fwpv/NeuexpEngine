#include "renderer.h"
#include "detail/polygon.h"
#include "detail/texture_data.h"
#include "camera.h"
#include "geometry.h"
#include "math2d.h"
#include "texture.h"
#include "transform.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <cstddef>
#include <optional>
#include <vector>

namespace Engine
{
namespace
{
bool valid(Color color)
{
    const auto component = [](float value) { return value >= 0 && value <= 1; };
    return component(color.red) && component(color.green) && component(color.blue) && component(color.alpha);
}

bool valid(ShapeFill fill)
{
    return fill == ShapeFill::Outline || fill == ShapeFill::Filled;
}

// Каждый проход получает одинаковое начальное состояние фиксированного конвейера.
void configureState()
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_TEXTURE_1D);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    glDisable(GL_TEXTURE_GEN_R);
    glDisable(GL_TEXTURE_GEN_Q);
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
    glShadeModel(GL_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
}

bool Renderer2D::initialize(RenderError& error)
{
    if (initialized) {
        error = RenderError::InvalidState;
        return false;
    }
    if (!glGetString(GL_VERSION)) {
        error = RenderError::NoCurrentContext;
        return false;
    }
    initialized = true;
    error = RenderError::None;
    return true;
}

bool Renderer2D::isInitialized() const
{
    return initialized;
}

void Renderer2D::shutdown()
{
    std::vector<Command>().swap(commands);
    for (const auto& texture : textures) {
        glDeleteTextures(1, &texture->id);
        texture->id = 0;
        texture->image.reset();
    }
    textures.clear();
    clip.reset();
    stage = Stage::Idle;
    frameWidth = 0;
    frameHeight = 0;
    initialized = false;
    firstError = RenderError::None;
}

void Renderer2D::beginFrame(int width, int height, Color clearColor)
{
    if (stage != Stage::Idle) {
        if (firstError == RenderError::None) {
            firstError = RenderError::InvalidState;
        }
        return;
    }
    firstError = RenderError::None;
    if (!initialized) {
        firstError = RenderError::InvalidState;
        return;
    }
    if (width <= 0 || height <= 0) {
        firstError = RenderError::InvalidFrameSize;
        return;
    }
    if (!valid(clearColor)) {
        firstError = RenderError::InvalidOptions;
        return;
    }
    frameWidth = width;
    frameHeight = height;
    stage = Stage::Frame;
    collectUnusedTextures();
    configureState();
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(clearColor.red, clearColor.green, clearColor.blue, clearColor.alpha);
    glClear(GL_COLOR_BUFFER_BIT);
    firstError = RenderError::None;
}

bool Renderer2D::selectView(const Viewport& area, const Affine2& transform, Stage next)
{
    if (stage == Stage::Idle || (stage == Stage::Ui && next == Stage::World)) {
        firstError = RenderError::InvalidState;
        return false;
    }
    const std::int64_t viewportY = static_cast<std::int64_t>(frameHeight) - area.y - area.height;
    if (area.width <= 0 || area.height <= 0
        || viewportY < std::numeric_limits<int>::min() || viewportY > std::numeric_limits<int>::max()) {
        firstError = RenderError::InvalidView;
        return false;
    }
    flush();
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
    applyClip(clip);
    return true;
}

void Renderer2D::applyClip(const std::optional<Viewport>& area) const
{
    using Wide = std::int64_t;
    Wide left = std::max<Wide>(0, viewport.x);
    Wide top = std::max<Wide>(0, viewport.y);
    Wide right = std::min<Wide>(frameWidth, static_cast<Wide>(viewport.x) + viewport.width);
    Wide bottom = std::min<Wide>(frameHeight, static_cast<Wide>(viewport.y) + viewport.height);
    if (area) {
        left = std::max<Wide>(left, area->x);
        top = std::max<Wide>(top, area->y);
        right = std::min<Wide>(right, static_cast<Wide>(area->x) + area->width);
        bottom = std::min<Wide>(bottom, static_cast<Wide>(area->y) + area->height);
    }
    glEnable(GL_SCISSOR_TEST);
    if (right <= left || bottom <= top) {
        glScissor(0, 0, 0, 0);
    } else {
        glScissor(static_cast<GLint>(left), static_cast<GLint>(frameHeight - bottom),
            static_cast<GLsizei>(right - left), static_cast<GLsizei>(bottom - top));
    }
}

void Renderer2D::setClip(const std::optional<Viewport>& area)
{
    if (!ready()) {
        return;
    }
    if (stage != Stage::World && stage != Stage::Ui) {
        firstError = RenderError::InvalidState;
        return;
    }
    if (area && (area->width < 0 || area->height < 0)) {
        firstError = RenderError::InvalidClip;
        return;
    }
    clip = area;
    applyClip(clip);
    firstError = RenderError::None;
}

bool Renderer2D::endFrame(RenderError& error)
{
    if (stage != Stage::Idle) {
        flush();
        glDisable(GL_SCISSOR_TEST);
        while (glGetError() != GL_NO_ERROR) {
            if (firstError == RenderError::None) {
                firstError = RenderError::Graphics;
            }
        }
        stage = Stage::Idle;
        clip.reset();
    } else if (firstError == RenderError::None) {
        firstError = RenderError::InvalidState;
    }
    error = firstError;
    return error == RenderError::None;
}

bool Renderer2D::prepareDraw(const DrawOptions& options)
{
    if (stage != Stage::World && stage != Stage::Ui) {
        firstError = RenderError::InvalidState;
        return false;
    }
    // NaN в ключе нарушает строгий порядок, необходимый std::stable_sort.
    if (!valid(options.color)
        || options.lineWidth <= 0
        || options.pointSize <= 0
        || !std::isfinite(options.order)) {
        firstError = RenderError::InvalidOptions;
        return false;
    }
    return true;
}

bool Renderer2D::submit(Primitive primitive, const Vector2* vertices, std::size_t count,
    const DrawOptions& options)
{
    if (!prepareDraw(options)) {
        return false;
    }
    if (options.color.alpha == 0 || count == 0) {
        return true;
    }
    DrawOptions screenOptions = options;
    screenOptions.transform = viewTransform * options.transform;
    commands.push_back(Command{primitive, std::vector<Vector2>(vertices, vertices + count),
        screenOptions, clip, std::nullopt, Texture{}, TextureSampling{}, std::nullopt, std::nullopt});
    return true;
}

void Renderer2D::execute(Primitive primitive, const Vector2* vertices, std::size_t count,
    const DrawOptions& options, const std::optional<std::array<Color, 2>>& gradient) const
{
    GLenum mode = GL_POINTS;
    switch (primitive) {
        case Primitive::Points: mode = GL_POINTS; break;
        case Primitive::Lines: mode = GL_LINES; break;
        case Primitive::LineLoop: mode = GL_LINE_LOOP; break;
        case Primitive::Quads: mode = GL_QUADS; break;
        case Primitive::TriangleFan: mode = GL_TRIANGLE_FAN; break;
        case Primitive::Triangles: mode = GL_TRIANGLES; break;
    }
    glColor4f(options.color.red, options.color.green, options.color.blue, options.color.alpha);
    glLineWidth(options.lineWidth);
    glPointSize(options.pointSize);
    glBegin(mode);
    for (std::size_t index = 0; index < count; ++index) {
        if (gradient) {
            const Color color = (*gradient)[index < 2 ? 0 : 1];
            glColor4f(color.red * options.color.red, color.green * options.color.green,
                color.blue * options.color.blue, color.alpha * options.color.alpha);
        }
        const Vector2 screen = options.transform.transformPoint(vertices[index]);
        glVertex2f(screen.x, screen.y);
    }
    glEnd();
}

void Renderer2D::flush()
{
    if (commands.empty()) {
        return;
    }
    std::stable_sort(commands.begin(), commands.end(), [](const Command& first, const Command& second) {
        if (first.options.layer != second.options.layer) {
            return first.options.layer < second.options.layer;
        }
        return first.options.order < second.options.order;
    });
    for (const Command& command : commands) {
        applyClip(command.clip);
        if (command.text) {
            executeText(command);
        } else if (command.texture.isValid()) {
            executeTexture(command);
        } else {
            execute(command.primitive, command.vertices.data(), command.vertices.size(), command.options, command.gradient);
        }
    }
    commands.clear();
    applyClip(clip);
}

void Renderer2D::drawPoint(Vector2 point, const DrawOptions& options)
{
    if (!ready()) {
        return;
    }
    submit(Primitive::Points, &point, 1, options);
}

void Renderer2D::drawLine(Vector2 start, Vector2 end, const DrawOptions& options)
{
    if (!ready()) {
        return;
    }
    const std::array<Vector2, 2> vertices = {start, end};
    submit(Primitive::Lines, vertices.data(), vertices.size(), options);
}

bool Renderer2D::queueRectangle(const Rect& rectangle, ShapeFill fill, const DrawOptions& options)
{
    if (rectangle.size.width < 0 || rectangle.size.height < 0 || !valid(fill)) {
        firstError = RenderError::InvalidGeometry;
        return false;
    }
    const Quad vertices = rectangle.corners();
    return submit(fill == ShapeFill::Filled ? Primitive::Quads : Primitive::LineLoop,
        vertices.data(), vertices.size(), options);
}

void Renderer2D::drawGradientRectangle(const Rect& rectangle, Color top, Color bottom,
    const DrawOptions& options)
{
    if (!ready()) {
        return;
    }
    if (!valid(top) || !valid(bottom)) {
        firstError = RenderError::InvalidOptions;
        return;
    }
    const auto previousSize = commands.size();
    if (!queueRectangle(rectangle, ShapeFill::Filled, options)) {
        return;
    }
    if (commands.size() > previousSize) {
        commands.back().gradient = std::array<Color, 2>{top, bottom};
    }
}

void Renderer2D::drawCircle(const Circle& circle, const DrawOptions& options, ShapeFill fill, unsigned int segments)
{
    if (!ready()) {
        return;
    }
    if (circle.radius < 0 || segments < 3 || !valid(fill)) {
        firstError = RenderError::InvalidGeometry;
        return;
    }
    if (!prepareDraw(options)) {
        return;
    }
    if (circle.radius == 0) {
        return;
    }
    std::vector<Vector2> vertices;
    if (fill == ShapeFill::Filled) {
        vertices.push_back(circle.center);
    }
    for (unsigned int index = 0; index < segments; ++index) {
        const float angle = 2 * Pi * (static_cast<float>(index) / static_cast<float>(segments));
        vertices.push_back(circle.center + Vector2{std::cos(angle), std::sin(angle)} * circle.radius);
    }
    if (fill == ShapeFill::Filled) {
        vertices.push_back(circle.center + Vector2{circle.radius, 0});
    }
    submit(fill == ShapeFill::Filled ? Primitive::TriangleFan : Primitive::LineLoop,
        vertices.data(), vertices.size(), options);
}

void Renderer2D::queuePolygon(const Vector2* vertices, std::size_t count, const DrawOptions& options, ShapeFill fill)
{
    if (!ready()) {
        return;
    }
    if (!valid(fill)) {
        firstError = RenderError::InvalidGeometry;
        return;
    }
    // Подготовка геометрии завершается до отправки команд: отказ не оставляет частичной заливки.
    std::vector<Vector2> contour;
    std::vector<std::array<std::size_t, 3>> triangles;
    if (!Detail::triangulatePolygon(vertices, count, contour, triangles)) {
        firstError = RenderError::InvalidGeometry;
        return;
    }
    if (fill == ShapeFill::Outline) {
        submit(Primitive::LineLoop, contour.data(), contour.size(), options);
        return;
    }
    std::vector<Vector2> points;
    for (const auto& triangle : triangles) {
        for (std::size_t index : triangle) {
            points.push_back(contour[index]);
        }
    }
    submit(Primitive::Triangles, points.data(), points.size(), options);
}
}
