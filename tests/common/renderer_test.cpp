#include "platform.h"
#include "renderer.h"
#include "platform_test_diagnostics.h"
#include "render_target.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <GL/glx.h>
#endif
#include <GL/gl.h>

#ifdef None
#undef None
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
constexpr GLsizei ImageSize = 256;
using RenderTest::require;
using RenderTest::FramebufferApi;
using RenderTest::RenderTarget;

using namespace Engine;
constexpr Color Black = { 0, 0, 0 };
constexpr Color Red = { 1, 0, 0 };
constexpr Color Green = { 0, 1, 0 };
constexpr Color Blue = { 0, 0, 1 };
constexpr Color White = { 1, 1, 1 };

DrawOptions style(Color color, const Transform2& transform = {})
{
    DrawOptions options;
    options.color = color;
    options.transform = transform.toAffine();
    return options;
}

void check(bool success, const RenderError& error)
{
    if (!success || error != RenderError::None) {
        throw std::runtime_error("Renderer operation failed, error=" + std::to_string(static_cast<int>(error)));
    }
    require(glGetError() == GL_NO_ERROR, "Renderer produced an OpenGL error.");
}

void pixel(int x, int y, Color expected)
{
    std::array<GLubyte, 3> actual = {};
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, ImageSize - 1 - y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, actual.data());
    require(glGetError() == GL_NO_ERROR, "Pixel read failed.");
    const std::array<float, 3> components = { expected.red, expected.green, expected.blue };
    for (std::size_t index = 0; index < actual.size(); ++index) {
        const int value = static_cast<int>(std::lround(255 * components[index]));
        if (std::abs(static_cast<int>(actual[index]) - value) > 2) {
            throw std::runtime_error("Unexpected pixel at " + std::to_string(x) + "," + std::to_string(y)
                + ": channel=" + std::to_string(index) + ", expected=" + std::to_string(value)
                + ", actual=" + std::to_string(actual[index]));
        }
    }
}

UiView screen(float scale = 1, Viewport area = { 0, 0, ImageSize, ImageSize })
{
    UiView view;
    UiError error;
    require(UiCanvas{ scale }.createView(area, view, error), "Could not create UI view.");
    return view;
}

void primitives()
{
    Renderer2D renderer;
    RenderError error;
    check(renderer.initialize(error), error);
    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setUiView(screen());
    renderer.drawRectangle({ 10, 10, 50, 40 }, style(Red), ShapeFill::Filled);
    renderer.drawRectangle({ 80.5F, 10.5F, 40, 40 }, style(Green), ShapeFill::Outline);
    renderer.drawCircle({ { 40, 100 }, 22 }, style(Blue), ShapeFill::Filled);
    renderer.drawCircle({ { 110, 100 }, 22 }, style(White), ShapeFill::Outline, 4);
    DrawOptions point = style(White);
    point.pointSize = 5;
    renderer.drawPoint({ 170.5F, 20.5F }, point);
    renderer.drawLine({ 155.5F, 55.5F }, { 220.5F, 55.5F }, style(Red));
    renderer.drawRectangle({ 150, 80, 60, 50 }, style(Red), ShapeFill::Filled);
    renderer.drawRectangle({ 175, 100, 60, 50 }, style({ 0, 0, 1, 0.5F }), ShapeFill::Filled);

    const std::vector<Vector2> concave = { { 10, 160 }, { 90, 160 }, { 90, 185 },
        { 35, 185 }, { 35, 235 }, { 10, 235 } };
    renderer.drawPolygon(concave, style(Green), ShapeFill::Filled);
    auto reversed = concave;
    std::reverse(reversed.begin(), reversed.end());
    renderer.drawPolygon(reversed, style(Red, { { 115, 0 }, 0, { 1, 1 }, {} }), ShapeFill::Filled);
    check(renderer.endFrame(error), error);
    pixel(15, 15, Red);
    pixel(59, 49, Red);
    pixel(60, 50, Black);
    pixel(80, 25, Green);
    pixel(100, 25, Black);
    pixel(40, 100, Blue);
    pixel(54, 110, Blue);
    pixel(62, 100, Black);
    pixel(110, 100, Black);
    pixel(120, 111, White);
    pixel(170, 20, White);
    pixel(165, 20, Black);
    pixel(180, 55, Red);
    pixel(180, 57, Black);
    pixel(190, 115, { 0.5F, 0, 0.5F });
    pixel(220, 115, { 0, 0, 0.5F });
    pixel(70, 170, Green);
    pixel(20, 220, Green);
    pixel(70, 220, Black);
    pixel(185, 170, Red);
    pixel(135, 220, Red);
    pixel(185, 220, Black);

    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setUiView(screen());
    renderer.drawPolygon(concave, style(White, { { 0.5F, 0.5F }, 0, { 1, 1 }, {} }), ShapeFill::Outline);
    check(renderer.endFrame(error), error);
    pixel(10, 200, White);
    pixel(70, 160, White);
    pixel(20, 200, Black);
    pixel(70, 200, Black);

    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setUiView(screen());
    renderer.drawPolygon(concave, style({ 1, 0, 0, 0.5F }), ShapeFill::Filled);
    check(renderer.endFrame(error), error);
    pixel(20, 200, { 0.5F, 0, 0 });
    pixel(70, 200, Black);
}

void gradient()
{
    Renderer2D renderer;
    RenderError error;
    check(renderer.initialize(error), error);
    glShadeModel(GL_FLAT);
    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setUiView(screen());
    renderer.drawGradientRectangle({ 10, 10, 80, 100 }, Red, Blue, style(White));
    renderer.setClip(Viewport{ 130, 20, 30, 50 });
    DrawOptions rotated = style({ 0, 1, 0, 0.5F }, { { 160, 20 }, Pi / 2, { 1, 1 }, {} });
    renderer.drawGradientRectangle({ 0, 0, 80, 40 }, White, Black, rotated);
    renderer.setClip(std::nullopt);
    DrawOptions foreground = style(Green);
    foreground.layer = 2;
    renderer.drawRectangle({ 10, 140, 80, 80 }, foreground, ShapeFill::Filled);
    DrawOptions background = style(White);
    background.layer = -1;
    renderer.drawGradientRectangle({ 10, 140, 80, 80 }, Red, Blue, background);
    // Обычная заливка не наследует цвета ранее выполненного градиента.
    renderer.drawRectangle({ 180, 20, 30, 30 }, style(Red), ShapeFill::Filled);
    check(renderer.endFrame(error), error);
    pixel(50, 10, { 0.995F, 0, 0.005F });
    pixel(50, 60, { 0.495F, 0, 0.505F });
    pixel(50, 109, { 0.005F, 0, 0.995F });
    pixel(50, 110, Black);
    pixel(140, 40, { 0, 0.25625F, 0 });
    pixel(125, 40, Black);
    pixel(150, 80, Black);
    pixel(50, 170, Green);
    pixel(190, 30, Red);
    pixel(190, 80, Black);
    renderer.shutdown();
}

void transforms()
{
    Renderer2D renderer;
    RenderError error;
    check(renderer.initialize(error), error);
    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setUiView(screen());
    const Transform2 transform = { { 100, 100 }, Pi / 2, { 2, 1 }, { 10, 5 } };
    renderer.drawRectangle({ 0, 0, 20, 10 }, style(Red, transform), ShapeFill::Filled);
    renderer.drawCircle({ {}, 10 }, style(Blue, { { 180, 80 }, Pi / 2, { 3, 1 }, {} }), ShapeFill::Filled);
    const std::vector<Vector2> triangle = { { 0, 0 }, { 40, 0 }, { 0, 40 } };
    renderer.drawPolygon(triangle, style(Green, { { 220, 160 }, 0, { -2, 1 }, {} }), ShapeFill::Filled);
    // Прямое аффинное преобразование с неперпендикулярными осями.
    DrawOptions shear = style(White);
    shear.transform = { 1, 1, 15, 0, 1, 190 };
    renderer.drawRectangle({ 0, 0, 20, 20 }, shear, ShapeFill::Filled);
    check(renderer.endFrame(error), error);
    pixel(100, 85, Red);
    pixel(100, 115, Red);
    pixel(90, 100, Black);
    pixel(100, 125, Black);
    pixel(180, 105, Blue);
    pixel(195, 80, Black);
    pixel(170, 165, Green);
    pixel(210, 190, Green);
    pixel(150, 195, Black);
    pixel(40, 205, White);
    pixel(17, 205, Black);
}

void views()
{
    Renderer2D renderer;
    RenderError error;
    check(renderer.initialize(error), error);
    Camera2 camera = { { 1000, 2000 }, 2, { 100, 100 }, ViewportMode::Fit };
    CameraView world;
    CameraError cameraError;
    require(camera.createView({ 20, 40, 200, 100 }, world, cameraError), "Camera creation failed.");
    const CameraView firstView = world;
    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setWorldView(world);
    renderer.drawRectangle({ 900, 1900, 200, 200 }, style(Red), ShapeFill::Filled);
    renderer.drawRectangle({ 999, 1999, 2, 2 }, style(White), ShapeFill::Filled);
    check(renderer.endFrame(error), error);
    pixel(70, 40, Red);
    pixel(169, 139, Red);
    pixel(69, 90, Black);
    pixel(170, 90, Black);
    pixel(120, 39, Black);
    pixel(120, 140, Black);
    pixel(120, 90, White);

    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setWorldView(world);
    renderer.drawRectangle({ 900, 1900, 200, 200 }, style(Red), ShapeFill::Filled);
    renderer.setClip(Viewport{ 100, 60, 30, 20 });
    renderer.drawRectangle({ 900, 1900, 200, 200 }, style(Green), ShapeFill::Filled);
    renderer.setClip(Viewport{ 110, 70, 0, 10 });
    renderer.drawRectangle({ 900, 1900, 200, 200 }, style(Blue), ShapeFill::Filled);
    check(renderer.endFrame(error), error);
    pixel(100, 60, Green);
    pixel(129, 79, Green);
    pixel(99, 60, Red);
    pixel(130, 79, Red);
    pixel(100, 80, Red);
    pixel(115, 75, Green);

    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setWorldView(firstView);
    renderer.drawRectangle({ 900, 1900, 200, 200 }, style(Red), ShapeFill::Filled);
    renderer.setClip(Viewport{ 0, 0, 0, 0 });
    // Новый мировой вид сбрасывает clip и использует другую камеру.
    camera.position = {};
    require(camera.createView({ 10, 160, 60, 60 }, world, cameraError), "Second camera failed.");
    renderer.setWorldView(world);
    renderer.drawRectangle({ -100, -100, 200, 200 }, style(Blue), ShapeFill::Filled);
    renderer.setUiView(screen(2, { 30, 20, 180, 180 }));
    renderer.drawRectangle({ 30, 20, 20, 20 }, style(White), ShapeFill::Filled);
    // Отрицательная область отсечения пересекается с viewport, а не расширяет его.
    renderer.setClip(Viewport{ -10, -20, 60, 60 });
    renderer.drawRectangle({ -100, -100, 500, 500 }, style(Green), ShapeFill::Filled);
    renderer.setClip(std::nullopt);
    renderer.drawRectangle({ 70, 70, 10, 10 }, style(Green), ShapeFill::Filled);
    check(renderer.endFrame(error), error);
    pixel(15, 165, Blue);
    pixel(9, 165, Black);
    pixel(90, 60, White);
    pixel(129, 99, White);
    pixel(130, 90, Red);
    pixel(30, 20, Green);
    pixel(49, 39, Green);
    pixel(50, 39, Black);
    pixel(29, 20, Black);
    pixel(175, 165, Green);

    // Следующий кадр очищает и поля, и прежнее отсечение; размер можно менять.
    renderer.beginFrame(128, 96, Blue);
    renderer.setUiView(screen(1, { 0, 0, 128, 96 }));
    renderer.drawRectangle({ 0, 0, 300, 300 }, style(Red), ShapeFill::Filled);
    check(renderer.endFrame(error), error);
    pixel(10, 10, Blue);
    pixel(200, 200, Blue);
    // При меньшем размере кадра начало отсчёта пикселей в FBO соответствует его нижней части.
    pixel(127, 255, Red);
    pixel(128, 255, Blue);
    pixel(0, 159, Blue);
    renderer.shutdown();
}

void errors()
{
    Renderer2D renderer;
    RenderError error;
    require(!renderer.endFrame(error) && error == RenderError::InvalidState, "End without frame accepted.");
    renderer.setUiView(screen());
    require(!renderer.endFrame(error) && error == RenderError::InvalidState, "UI without frame accepted.");
    check(renderer.initialize(error), error);
    renderer.beginFrame(0, ImageSize, Black);
    require(!renderer.endFrame(error) && error == RenderError::InvalidFrameSize, "Zero frame accepted.");
    renderer.beginFrame(ImageSize, ImageSize, { 1, 0, 0, 2 });
    require(!renderer.endFrame(error) && error == RenderError::InvalidOptions, "Invalid clear color accepted.");
    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.beginFrame(ImageSize, ImageSize, White);
    require(!renderer.endFrame(error) && error == RenderError::InvalidState, "Nested frame accepted.");
    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.drawPoint({}, Red);
    require(!renderer.endFrame(error) && error == RenderError::InvalidState, "Drawing without view accepted.");
    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setUiView({});
    require(!renderer.endFrame(error) && error == RenderError::InvalidView, "Invalid view accepted.");
    const auto reject = [&](const auto& operation, RenderError expected, const char* message) {
        renderer.beginFrame(ImageSize, ImageSize, Black);
        renderer.setUiView(screen());
        operation();
        require(!renderer.endFrame(error) && error == expected, message);
    };
    const std::vector<std::vector<Vector2>> invalid = {
        {}, { { 10, 10 }, { 20, 20 } }, { { 10, 10 }, { 20, 20 }, { 30, 30 } },
        { { 10, 10 }, { 60, 60 }, { 10, 60 }, { 60, 10 } },
        { { 10, 10 }, { 60, 10 }, { 30, 10 }, { 30, 60 }, { 10, 60 } },
        { { 10, 10 }, { 60, 10 }, { 60, 60 }, { 30, 10 }, { 10, 60 } },
        { { 10, 10 }, { 60, 10 }, { 60, 60 }, { 10, 10 }, { 10, 60 } }
    };
    for (const auto& polygon : invalid) {
        for (ShapeFill fill : { ShapeFill::Filled, ShapeFill::Outline }) {
            reject([&] { renderer.drawPolygon(polygon, style(Red), fill); }, RenderError::InvalidGeometry, "Invalid polygon accepted.");
        }
    }
    reject([&] { renderer.drawCircle({ {}, -1 }, style(Red), ShapeFill::Filled); }, RenderError::InvalidGeometry, "Negative radius accepted.");
    reject([&] { renderer.drawCircle({ {}, 10 }, style(Red), ShapeFill::Filled, 2); }, RenderError::InvalidGeometry, "Invalid circle subdivision accepted.");
    reject([&] { renderer.drawRectangle({ 10, 10, -1, 20 }, style(Red), ShapeFill::Filled); }, RenderError::InvalidGeometry, "Negative rectangle accepted.");
    DrawOptions options = style(Red);
    options.lineWidth = 0;
    reject([&] { renderer.drawLine({}, { 50, 50 }, options); },
        RenderError::InvalidOptions, "Zero line width accepted.");
    options = style(Red);
    options.transform.m02 = std::numeric_limits<float>::infinity();
    reject([&] { renderer.drawRectangle({ 0, 0, 50, 50 }, options); },
        RenderError::InvalidOptions, "Invalid transform accepted.");
    reject([&] { renderer.setClip(Viewport{ 0, 0, -1, 10 }); },
        RenderError::InvalidClip, "Invalid clip accepted.");

    reject([&] { renderer.drawGradientRectangle({ 180, 70, 30, 30 }, { 2, 0, 0 }, White, style(White)); }, RenderError::InvalidOptions, "Invalid gradient color was accepted.");
    reject([&] { renderer.drawGradientRectangle({ 180, 70, 30, 30 }, White, { 0, 0, std::numeric_limits<float>::quiet_NaN() }, style(White)); }, RenderError::InvalidOptions, "Nonfinite gradient color was accepted.");

    for (float invalid : { std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() }) {
        options = style(Red);
        options.order = invalid;
        reject([&] { renderer.drawRectangle({ 0, 0, 100, 100 }, options); },
            RenderError::InvalidOptions, "Nonfinite sort key accepted.");
    }
    Camera2 camera;
    CameraView world;
    CameraError cameraError;
    require(camera.createView({ 0, 0, ImageSize, ImageSize }, world, cameraError), "Camera failed.");
    reject([&] { renderer.setWorldView(world); }, RenderError::InvalidState, "World drawing after UI accepted.");
    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setUiView(screen());
    pixel(20, 20, Black);
    // Корректные вырожденные размеры и замкнутый контур разрешены.
    const std::vector<Vector2> closed = { { 10, 10 }, { 10, 10 }, { 30, 10 }, { 60, 10 },
        { 60, 60 }, { 10, 60 }, { 10, 10 } };
    renderer.drawPolygon(closed, style(Green), ShapeFill::Filled);
    renderer.drawCircle({ { 100, 100 }, 0 }, style(Red), ShapeFill::Filled);
    renderer.drawRectangle({ 100, 100, 0, 10 }, style(Red), ShapeFill::Filled);
    check(renderer.endFrame(error), error);
    pixel(20, 20, Green);
    pixel(50, 50, Green);
    pixel(100, 100, Black);

    renderer.drawPoint({}, Red);
    require(!renderer.endFrame(error) && error == RenderError::InvalidState, "Drawing after end accepted.");
    renderer.beginFrame(ImageSize, ImageSize, Blue);
    check(renderer.endFrame(error), error);
    pixel(20, 20, Blue);
    renderer.shutdown();
}

void state()
{
    Renderer2D renderer;
    RenderError error;
    check(renderer.initialize(error), error);
    // Состояние, которое часто остаётся от другого рисования в том же контексте.
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 0, 0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_NEVER, 0);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NEVER, 0, 0);
    glEnable(GL_LIGHTING);
    glEnable(GL_FOG);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT_AND_BACK);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glScalef(0, 0, 0);
    renderer.beginFrame(ImageSize, ImageSize, Blue);
    renderer.setUiView(screen());
    renderer.drawRectangle({ 10, 10, 80, 80 }, style(Red), ShapeFill::Filled);
    renderer.drawRectangle({ 10, 10, 80, 80 }, style({ 0, 1, 0, 0.5F }), ShapeFill::Filled);
    check(renderer.endFrame(error), error);
    pixel(200, 200, Blue);
    pixel(100, 100, Blue);
    pixel(40, 40, { 0.5F, 0.5F, 0 });

    require(glIsEnabled(GL_SCISSOR_TEST) == GL_FALSE, "Frame end left clipping enabled.");
}

CameraView screenWorld(Viewport area = { 0, 0, ImageSize, ImageSize })
{
    Camera2 camera;
    camera.referenceSize = { static_cast<float>(area.width), static_cast<float>(area.height) };
    camera.position = { static_cast<float>(area.width) / 2, static_cast<float>(area.height) / 2 };
    CameraView view;
    CameraError error;
    require(camera.createView(area, view, error), "Sorting camera failed.");
    return view;
}

DrawOptions ordered(Color color, int layer, float order = 0)
{
    DrawOptions options = style(color);
    options.layer = layer;
    options.order = order;
    return options;
}

void sorting()
{
    Renderer2D renderer;
    RenderError error;
    check(renderer.initialize(error), error);
    for (bool reversed : { false, true }) {
        renderer.beginFrame(ImageSize, ImageSize, Black);
        renderer.setWorldView(screenWorld());
        for (int step = 0; step < 2; ++step) {
            const bool first = (step == 0) != reversed;
            // Слой имеет приоритет над ключом внутри слоя.
            renderer.drawRectangle({ 10, 10, 80, 80 }, ordered(first ? Red : Blue, first ? -2 : -1, first ? 100 : -100), ShapeFill::Filled);
            // Равные ключи сохраняют порядок подачи команд.
            renderer.drawRectangle({ 110, 10, 50, 50 }, style(first ? Red : Green), ShapeFill::Filled);
            // Сортировка внутри одного слоя, включая отрицательные значения.
            renderer.drawRectangle({ 200, 10, 40, 40 }, ordered(first ? Red : Blue, 4, first ? 50 : -20), ShapeFill::Filled);
            // Прозрачный фон должен оказаться позади непрозрачного объекта.
            renderer.drawRectangle({ 10, 110, 80, 80 }, ordered(first ? Color{ 0, 0, 1, 0.5F } : Green, first ? -1 : 1), ShapeFill::Filled);
            // Прозрачный передний слой смешивается с непрозрачным фоном.
            renderer.drawRectangle({ 110, 110, 80, 80 }, ordered(first ? Red : Color{ 0, 0, 1, 0.5F }, first ? -1 : 1), ShapeFill::Filled);
        }
        renderer.drawRectangle({ 200, 110, 40, 40 }, style({ 1, 0, 0, 0.5F }), ShapeFill::Filled);
        renderer.drawRectangle({ 200, 110, 40, 40 }, style({ 0, 0, 1, 0.5F }), ShapeFill::Filled);
        renderer.setUiView(screen());
        renderer.drawRectangle({ 25, 25, 10, 10 }, ordered(White, std::numeric_limits<int>::min()), ShapeFill::Filled);
        check(renderer.endFrame(error), error);
        pixel(20, 20, Blue);
        pixel(30, 30, White);
        pixel(120, 20, reversed ? Red : Green);
        pixel(210, 20, Red);
        pixel(20, 120, Green);
        pixel(120, 120, { 0.5F, 0, 0.5F });
        pixel(210, 120, { 0.25F, 0, 0.5F });
    }

    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setWorldView(screenWorld());
    renderer.setClip(Viewport{ 0, 0, 40, 100 });
    renderer.drawRectangle({ 0, 0, 100, 100 }, ordered(Red, 2), ShapeFill::Filled);
    renderer.setClip(Viewport{ 60, 0, 40, 100 });
    renderer.drawRectangle({ 0, 0, 100, 100 }, ordered(Blue, 1), ShapeFill::Filled);
    renderer.setClip(std::nullopt);
    // Отложенные команды владеют копией вершин и параметров.
    std::vector<Vector2> vertices = { { 10, 120 }, { 70, 120 }, { 10, 180 } };
    DrawOptions options = style(Blue);
    renderer.drawPolygon(vertices, options, ShapeFill::Filled);
    vertices.clear();
    options.color = Green;
    options.transform.m02 = 1000;
    check(renderer.endFrame(error), error);
    pixel(20, 20, Red);
    pixel(50, 20, Black);
    pixel(80, 20, Blue);
    pixel(20, 130, Blue);

    // Границы видов старше ключей сортировки; UI всегда идёт после мира.
    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setWorldView(screenWorld());
    renderer.drawRectangle({ 0, 0, 100, 100 }, ordered(Red, std::numeric_limits<int>::max()), ShapeFill::Filled);
    renderer.setWorldView(screenWorld());
    renderer.drawRectangle({ 0, 0, 100, 100 }, ordered(Blue, std::numeric_limits<int>::min()), ShapeFill::Filled);
    renderer.setUiView(screen());
    renderer.drawRectangle({ 10, 10, 20, 20 }, ordered(White, std::numeric_limits<int>::min()), ShapeFill::Filled);
    renderer.drawRectangle({ 110, 10, 40, 40 }, ordered(Red, 10), ShapeFill::Filled);
    renderer.drawRectangle({ 110, 10, 40, 40 }, ordered(Green, 1), ShapeFill::Filled);
    check(renderer.endFrame(error), error);
    pixel(80, 80, Blue);
    pixel(20, 20, White);
    pixel(120, 20, Red);

    // Отмена кадра не переносит его команды в следующий запуск рендерера.
    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setWorldView(screenWorld());
    renderer.drawRectangle({ 10, 10, 80, 80 }, style(Red), ShapeFill::Filled);
    renderer.shutdown();
    check(renderer.initialize(error), error);
    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setUiView(screen());
    renderer.drawRectangle({ 10, 10, 30, 30 }, style(Blue), ShapeFill::Filled);
    check(renderer.endFrame(error), error);
    pixel(20, 20, Blue);
    pixel(80, 80, Black);
    renderer.shutdown();
}

void ellipses()
{
    Renderer2D renderer;
    RenderError error;
    check(renderer.initialize(error), error);
    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setUiView(screen());
    renderer.drawEllipse({ 40, 40 }, { 20, 10 }, Green, ShapeFill::Filled, 64);
    const DrawOptions transformed = style(Red, { { 100, 80 }, Pi / 2, { 2, 1 }, {} });
    renderer.drawEllipse({ 5, 0 }, { 12, 5 }, transformed, ShapeFill::Filled, 64);
    renderer.drawRectangle({ 150, 20, 20, 20 }, Blue);
    renderer.drawEllipse({ 65, 140 }, { 20, 10 }, White, ShapeFill::Outline, 4);
    renderer.drawPolygon({ { 5, 170 }, { 25, 170 }, { 5, 190 } }, Green);
    renderer.drawLine({ 150.5F, 60.5F }, { 190.5F, 60.5F });
    DrawOptions point = style(Red);
    point.pointSize = 5;
    renderer.drawPoint({ 180.5F, 100.5F }, point);
    check(renderer.endFrame(error), error);
    pixel(40, 40, Green);
    pixel(55, 40, Green);
    pixel(40, 55, Black);
    pixel(100, 90, Red);
    pixel(100, 110, Red);
    pixel(108, 90, Black);
    pixel(155, 25, Blue);
    pixel(65, 140, Black);
    pixel(75, 144, White);
    pixel(7, 172, Green);
    pixel(170, 60, White);
    pixel(180, 100, Red);
    renderer.shutdown();
}

void debugText()
{
    Renderer2D renderer;
    RenderError error;
    check(renderer.initialize(error), error);
    Image image;
    image.width = 1;
    image.height = 1;
    image.pixels = { 0, 0, 255, 255 };
    Texture texture;
    ResourceError resourceError;
    require(renderer.createTexture(image, texture, resourceError), "Renderer texture creation failed.");
    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setWorldView(screenWorld());
    renderer.drawRectangle({ 80, 80, 30, 30 }, ordered(Red, 5));
    renderer.drawRectangle({ 80, 80, 30, 30 }, ordered(Blue, -5));
    renderer.setUiView(screen());
    renderer.setClip(Viewport{ 20, 20, 3, 20 });
    renderer.drawDebugText("A", { 20, 20 }, Green, 2);
    renderer.setClip(std::nullopt);
    renderer.drawDebugText("A?9+-/", {}, style(White, { { 120, 20 }, 0, { 1, 1 }, {} }), 2);
    renderer.drawTexture(texture, { 150, 80, 30, 30 });
    renderer.drawGradientRectangle({ 180, 150, 30, 40 }, Red, Red);
    check(renderer.endFrame(error), error);
    pixel(85, 85, Red);
    pixel(22, 20, Green);
    pixel(20, 24, Green);
    pixel(24, 24, Black);
    pixel(122, 20, White);
    pixel(120, 20, Black);
    pixel(130, 20, Black);
    pixel(136, 20, White);
    pixel(146, 24, White);
    pixel(154, 24, White);
    pixel(160, 26, White);
    pixel(160, 90, Blue);
    pixel(190, 170, Red);
    renderer.shutdown();
}

void frameErrors()
{
    Renderer2D renderer;
    RenderError error;
    check(renderer.initialize(error), error);
    // Ошибка текущего контекста не должна превращаться в успешное завершение кадра.
    glEnable(static_cast<GLenum>(0xffffffffU));
    renderer.beginFrame(ImageSize, ImageSize, Black);
    require(!renderer.endFrame(error) && error == RenderError::Graphics, "OpenGL error was silently ignored.");
    renderer.beginFrame(ImageSize, ImageSize, Black);
    check(renderer.endFrame(error), error);
    renderer.drawPoint({}, White);
    require(!renderer.endFrame(error) && error == RenderError::InvalidState, "Renderer accepted drawing outside a frame.");
    renderer.beginFrame(0, ImageSize, Black);
    renderer.setUiView(screen());
    renderer.drawEllipse({}, { -1, 2 }, Red);
    require(!renderer.endFrame(error) && error == RenderError::InvalidFrameSize, "Begin-frame error was overwritten.");

    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setUiView(screen());
    renderer.drawRectangle({ 10, 10, 20, 20 }, Red);
    renderer.drawEllipse({ 40, 40 }, { -1, 5 }, White);
    renderer.drawRectangle({ 10, 10, 20, 20 }, Blue);
    renderer.setClip(Viewport{ 0, 0, -1, 5 });
    require(!renderer.endFrame(error) && error == RenderError::InvalidGeometry, "First draw error was overwritten.");
    pixel(15, 15, Red);

    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setUiView(screen());
    renderer.drawRectangle({ 10, 10, 20, 20 }, Blue);
    check(renderer.endFrame(error), error);
    pixel(15, 15, Blue);

    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.beginFrame(ImageSize, ImageSize, Red);
    require(!renderer.endFrame(error) && error == RenderError::InvalidState, "Nested Renderer frame was accepted.");
    renderer.beginFrame(ImageSize, ImageSize, Green);
    check(renderer.endFrame(error), error);

    for (float pixelSize : { 0.0F, -1.0F, std::numeric_limits<float>::infinity() }) {
        renderer.beginFrame(ImageSize, ImageSize, Black);
        renderer.setUiView(screen());
        renderer.drawDebugText("A", {}, White, pixelSize);
        require(!renderer.endFrame(error) && error == RenderError::InvalidGeometry, "Invalid debug-text size was accepted.");
    }
    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setUiView(screen());
    renderer.drawEllipse({}, { 1, 1 }, White, ShapeFill::Filled, 2);
    require(!renderer.endFrame(error) && error == RenderError::InvalidGeometry, "Invalid ellipse segments were accepted.");
    renderer.beginFrame(ImageSize, ImageSize, Black);
    renderer.setUiView(screen());
    renderer.drawDebugText("A", {}, Color{ 2, 0, 0 });
    require(!renderer.endFrame(error) && error == RenderError::InvalidOptions, "Debug text ignored invalid options.");
    renderer.shutdown();
}

int runTest(const std::string& mode)
{
    FramebufferApi api;
    if (!api.load()) {
        std::cout << "Pixel checks require optional FBO support from the test driver.\n";
        return 77;
    }
    RenderTarget target(api, ImageSize);
    target.initialize();
    if (mode == "primitives") { primitives(); }
    else if (mode == "gradient") { gradient(); }
    else if (mode == "transforms") { transforms(); }
    else if (mode == "views") { views(); }
    else if (mode == "errors") { errors(); }
    else if (mode == "state") { state(); }
    else if (mode == "sorting") { sorting(); }
    else if (mode == "ellipses") {
        ellipses();
    }
    else if (mode == "debug_text") {
        debugText();
    }
    else if (mode == "frame_errors") {
        frameErrors();
    }
    else { throw std::runtime_error("Unknown renderer test mode."); }
    return 0;
}
}

int main(int argc, char** argv)
{
#ifndef _WIN32
    const char* displayName = std::getenv("DISPLAY");
    if (!displayName || !*displayName) {
        std::cout << "Renderer pixel checks require an X11 display.\n";
        return 77;
    }
#endif
    int result = 1;
    try {
        require(argc == 2, "Expected a renderer test mode.");
        Platform::WindowOptions options;
        options.width = ImageSize;
        options.height = ImageSize;
        options.title = "Renderer pixel checks";
        std::vector<Platform::Error> platformErrors;
        const bool initialized = Platform::initialize(options, platformErrors);
        PlatformTest::reportErrors(platformErrors);
        require(initialized, "Could not create test context.");
        result = runTest(argv[1]);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
    }
    std::vector<Platform::Error> platformErrors;
    Platform::shutdown(platformErrors);
    PlatformTest::reportErrors(platformErrors);
    return platformErrors.empty() ? result : 1;
}
