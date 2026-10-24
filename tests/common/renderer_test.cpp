#include "platform.h"
#include "renderer.h"
#include "platform_test_diagnostics.h"

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
constexpr GLenum Framebuffer = 0x8D40;
constexpr GLenum ColorAttachment = 0x8CE0;
constexpr GLenum FramebufferComplete = 0x8CD5;
constexpr GLsizei ImageSize = 256;

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template<class Function>
Function loadFunction(const std::string& name)
{
#ifdef _WIN32
    const auto address = wglGetProcAddress(name.c_str());
    // Некоторые WGL-драйверы возвращают служебные значения вместо nullptr.
    const auto value = reinterpret_cast<std::intptr_t>(address);
    if (value == 0 || value == 1 || value == 2 || value == 3 || value == -1) {
        return nullptr;
    }
    return reinterpret_cast<Function>(reinterpret_cast<void (WINAPI*)()>(address));
#else
    const auto address = glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name.c_str()));
    return reinterpret_cast<Function>(address);
#endif
}

struct FramebufferApi
{
    using Generate = void (APIENTRY*)(GLsizei, GLuint*);
    using Bind = void (APIENTRY*)(GLenum, GLuint);
    using AttachTexture = void (APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
    using CheckStatus = GLenum (APIENTRY*)(GLenum);
    using Delete = void (APIENTRY*)(GLsizei, const GLuint*);

    Generate generate = nullptr;
    Bind bind = nullptr;
    AttachTexture attachTexture = nullptr;
    CheckStatus checkStatus = nullptr;
    Delete remove = nullptr;

    bool load()
    {
        const auto* versionText = glGetString(GL_VERSION);
        const auto* extensionText = glGetString(GL_EXTENSIONS);
        require(versionText != nullptr && extensionText != nullptr, "Could not query the OpenGL context.");
        int major = 0;
        std::istringstream version(reinterpret_cast<const char*>(versionText));
        version >> major;
        const std::string extensions = " " + std::string(reinterpret_cast<const char*>(extensionText)) + " ";
        const bool core = major >= 3 || extensions.find(" GL_ARB_framebuffer_object ") != std::string::npos;
        const bool ext = extensions.find(" GL_EXT_framebuffer_object ") != std::string::npos;
        if (!core && !ext) {
            return false;
        }
        const std::string suffix = core ? "" : "EXT";
        generate = loadFunction<Generate>("glGenFramebuffers" + suffix);
        bind = loadFunction<Bind>("glBindFramebuffer" + suffix);
        attachTexture = loadFunction<AttachTexture>("glFramebufferTexture2D" + suffix);
        checkStatus = loadFunction<CheckStatus>("glCheckFramebufferStatus" + suffix);
        remove = loadFunction<Delete>("glDeleteFramebuffers" + suffix);
        require(generate && bind && attachTexture && checkStatus && remove,
            "The driver advertises FBO support but does not expose its functions.");
        return true;
    }
};

struct RenderTarget
{
    const FramebufferApi& api;
    GLuint framebuffer = 0;
    GLuint texture = 0;

    explicit RenderTarget(const FramebufferApi& functions)
        : api(functions)
    {
    }

    ~RenderTarget()
    {
        api.bind(Framebuffer, 0);
        api.remove(1, &framebuffer);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &texture);
    }

    void initialize()
    {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ImageSize, ImageSize, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        api.generate(1, &framebuffer);
        api.bind(Framebuffer, framebuffer);
        api.attachTexture(Framebuffer, ColorAttachment, GL_TEXTURE_2D, texture, 0);
        glDrawBuffer(ColorAttachment);
        glReadBuffer(ColorAttachment);
        require(api.checkStatus(Framebuffer) == FramebufferComplete, "The test FBO is incomplete.");
        require(glGetError() == GL_NO_ERROR, "Could not initialize the test FBO.");
    }
};

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
    check(renderer.beginFrame(ImageSize, ImageSize, Black, error), error);
    check(renderer.beginUi(screen(), error), error);
    check(renderer.drawRectangle({ 10, 10, 50, 40 }, ShapeFill::Filled, style(Red), error), error);
    check(renderer.drawRectangle({ 80.5F, 10.5F, 40, 40 }, ShapeFill::Outline, style(Green), error), error);
    check(renderer.drawCircle({ { 40, 100 }, 22 }, ShapeFill::Filled, style(Blue), error), error);
    check(renderer.drawCircle({ { 110, 100 }, 22 }, ShapeFill::Outline, style(White), error, 4), error);
    DrawOptions point = style(White);
    point.pointSize = 5;
    check(renderer.drawPoint({ 170.5F, 20.5F }, point, error), error);
    check(renderer.drawLine({ 155.5F, 55.5F }, { 220.5F, 55.5F }, style(Red), error), error);
    check(renderer.drawRectangle({ 150, 80, 60, 50 }, ShapeFill::Filled, style(Red), error), error);
    check(renderer.drawRectangle({ 175, 100, 60, 50 }, ShapeFill::Filled, style({ 0, 0, 1, 0.5F }), error), error);

    const std::vector<Vector2> concave = { { 10, 160 }, { 90, 160 }, { 90, 185 },
        { 35, 185 }, { 35, 235 }, { 10, 235 } };
    check(renderer.drawPolygon(concave, ShapeFill::Filled, style(Green), error), error);
    auto reversed = concave;
    std::reverse(reversed.begin(), reversed.end());
    check(renderer.drawPolygon(reversed, ShapeFill::Filled,
        style(Red, { { 115, 0 }, 0, { 1, 1 }, {} }), error), error);
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

    check(renderer.beginFrame(ImageSize, ImageSize, Black, error), error);
    check(renderer.beginUi(screen(), error), error);
    check(renderer.drawPolygon(concave, ShapeFill::Outline,
        style(White, { { 0.5F, 0.5F }, 0, { 1, 1 }, {} }), error), error);
    check(renderer.endFrame(error), error);
    pixel(10, 200, White);
    pixel(70, 160, White);
    pixel(20, 200, Black);
    pixel(70, 200, Black);

    check(renderer.beginFrame(ImageSize, ImageSize, Black, error), error);
    check(renderer.beginUi(screen(), error), error);
    check(renderer.drawPolygon(concave, ShapeFill::Filled, style({ 1, 0, 0, 0.5F }), error), error);
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
    check(renderer.beginFrame(ImageSize, ImageSize, Black, error), error);
    check(renderer.beginUi(screen(), error), error);
    check(renderer.drawGradientRectangle({ 10, 10, 80, 100 }, Red, Blue, style(White), error), error);
    check(renderer.setClip(Viewport{ 130, 20, 30, 50 }, error), error);
    DrawOptions rotated = style({ 0, 1, 0, 0.5F }, { { 160, 20 }, Pi / 2, { 1, 1 }, {} });
    check(renderer.drawGradientRectangle({ 0, 0, 80, 40 }, White, Black, rotated, error), error);
    check(renderer.setClip(std::nullopt, error), error);
    DrawOptions foreground = style(Green);
    foreground.layer = 2;
    check(renderer.drawRectangle({ 10, 140, 80, 80 }, ShapeFill::Filled, foreground, error), error);
    DrawOptions background = style(White);
    background.layer = -1;
    check(renderer.drawGradientRectangle({ 10, 140, 80, 80 }, Red, Blue, background, error), error);
    // Обычная заливка не наследует цвета ранее выполненного градиента.
    check(renderer.drawRectangle({ 180, 20, 30, 30 }, ShapeFill::Filled, style(Red), error), error);
    require(!renderer.drawGradientRectangle({ 180, 70, 30, 30 }, { 2, 0, 0 }, White, style(White), error)
        && error == RenderError::InvalidOptions, "Invalid gradient color was accepted.");
    require(!renderer.drawGradientRectangle({ 180, 70, 30, 30 }, White,
        { 0, 0, std::numeric_limits<float>::quiet_NaN() }, style(White), error)
        && error == RenderError::InvalidOptions, "Nonfinite gradient color was accepted.");
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
    check(renderer.beginFrame(ImageSize, ImageSize, Black, error), error);
    check(renderer.beginUi(screen(), error), error);
    const Transform2 transform = { { 100, 100 }, Pi / 2, { 2, 1 }, { 10, 5 } };
    check(renderer.drawRectangle({ 0, 0, 20, 10 }, ShapeFill::Filled, style(Red, transform), error), error);
    check(renderer.drawCircle({ {}, 10 }, ShapeFill::Filled,
        style(Blue, { { 180, 80 }, Pi / 2, { 3, 1 }, {} }), error), error);
    const std::vector<Vector2> triangle = { { 0, 0 }, { 40, 0 }, { 0, 40 } };
    check(renderer.drawPolygon(triangle, ShapeFill::Filled,
        style(Green, { { 220, 160 }, 0, { -2, 1 }, {} }), error), error);
    // Прямое аффинное преобразование с неперпендикулярными осями.
    DrawOptions shear = style(White);
    shear.transform = { 1, 1, 15, 0, 1, 190 };
    check(renderer.drawRectangle({ 0, 0, 20, 20 }, ShapeFill::Filled, shear, error), error);
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
    check(renderer.beginFrame(ImageSize, ImageSize, Black, error), error);
    check(renderer.beginWorld(world, error), error);
    check(renderer.drawRectangle({ 900, 1900, 200, 200 }, ShapeFill::Filled, style(Red), error), error);
    check(renderer.drawRectangle({ 999, 1999, 2, 2 }, ShapeFill::Filled, style(White), error), error);
    check(renderer.endFrame(error), error);
    pixel(70, 40, Red);
    pixel(169, 139, Red);
    pixel(69, 90, Black);
    pixel(170, 90, Black);
    pixel(120, 39, Black);
    pixel(120, 140, Black);
    pixel(120, 90, White);

    check(renderer.beginFrame(ImageSize, ImageSize, Black, error), error);
    check(renderer.beginWorld(world, error), error);
    check(renderer.drawRectangle({ 900, 1900, 200, 200 }, ShapeFill::Filled, style(Red), error), error);
    check(renderer.setClip(Viewport{ 100, 60, 30, 20 }, error), error);
    check(renderer.drawRectangle({ 900, 1900, 200, 200 }, ShapeFill::Filled, style(Green), error), error);
    check(renderer.setClip(Viewport{ 110, 70, 0, 10 }, error), error);
    check(renderer.drawRectangle({ 900, 1900, 200, 200 }, ShapeFill::Filled, style(Blue), error), error);
    check(renderer.endFrame(error), error);
    pixel(100, 60, Green);
    pixel(129, 79, Green);
    pixel(99, 60, Red);
    pixel(130, 79, Red);
    pixel(100, 80, Red);
    pixel(115, 75, Green);

    check(renderer.beginFrame(ImageSize, ImageSize, Black, error), error);
    check(renderer.beginWorld(firstView, error), error);
    check(renderer.drawRectangle({ 900, 1900, 200, 200 }, ShapeFill::Filled, style(Red), error), error);
    check(renderer.setClip(Viewport{ 0, 0, 0, 0 }, error), error);
    // Новый мировой вид сбрасывает clip и использует другую камеру.
    camera.position = {};
    require(camera.createView({ 10, 160, 60, 60 }, world, cameraError), "Second camera failed.");
    check(renderer.beginWorld(world, error), error);
    check(renderer.drawRectangle({ -100, -100, 200, 200 }, ShapeFill::Filled, style(Blue), error), error);
    check(renderer.beginUi(screen(2, { 30, 20, 180, 180 }), error), error);
    check(renderer.drawRectangle({ 30, 20, 20, 20 }, ShapeFill::Filled, style(White), error), error);
    require(!renderer.beginWorld(world, error) && error == RenderError::InvalidState,
        "World drawing after UI was accepted.");
    // Отрицательная область отсечения пересекается с viewport, а не расширяет его.
    check(renderer.setClip(Viewport{ -10, -20, 60, 60 }, error), error);
    check(renderer.drawRectangle({ -100, -100, 500, 500 }, ShapeFill::Filled, style(Green), error), error);
    check(renderer.setClip(std::nullopt, error), error);
    check(renderer.drawRectangle({ 70, 70, 10, 10 }, ShapeFill::Filled, style(Green), error), error);
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
    check(renderer.beginFrame(128, 96, Blue, error), error);
    check(renderer.beginUi(screen(1, { 0, 0, 128, 96 }), error), error);
    check(renderer.drawRectangle({ 0, 0, 300, 300 }, ShapeFill::Filled, style(Red), error), error);
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
    require(!renderer.beginUi(screen(), error) && error == RenderError::InvalidState, "UI without frame accepted.");
    check(renderer.initialize(error), error);
    require(!renderer.beginFrame(0, ImageSize, Black, error) && error == RenderError::InvalidFrameSize,
        "Zero frame accepted.");
    require(!renderer.beginFrame(ImageSize, ImageSize, { 1, 0, 0, 2 }, error)
        && error == RenderError::InvalidOptions, "Invalid clear color accepted.");
    check(renderer.beginFrame(ImageSize, ImageSize, Black, error), error);
    require(!renderer.beginFrame(ImageSize, ImageSize, White, error) && error == RenderError::InvalidState,
        "Nested frame accepted.");
    require(!renderer.drawPoint({}, style(Red), error) && error == RenderError::InvalidState,
        "Drawing without view accepted.");
    require(!renderer.beginUi({}, error) && error == RenderError::InvalidView, "Invalid view accepted.");
    check(renderer.beginUi(screen(), error), error);
    const std::vector<std::vector<Vector2>> invalid = {
        {}, { { 10, 10 }, { 20, 20 } }, { { 10, 10 }, { 20, 20 }, { 30, 30 } },
        { { 10, 10 }, { 60, 60 }, { 10, 60 }, { 60, 10 } },
        { { 10, 10 }, { 60, 10 }, { 30, 10 }, { 30, 60 }, { 10, 60 } },
        { { 10, 10 }, { 60, 10 }, { 60, 60 }, { 30, 10 }, { 10, 60 } },
        { { 10, 10 }, { 60, 10 }, { 60, 60 }, { 10, 10 }, { 10, 60 } }
    };
    for (const auto& polygon : invalid) {
        for (ShapeFill fill : { ShapeFill::Filled, ShapeFill::Outline }) {
            require(!renderer.drawPolygon(polygon, fill, style(Red), error)
                && error == RenderError::InvalidGeometry, "Invalid polygon accepted.");
        }
    }
    require(!renderer.drawCircle({ {}, -1 }, ShapeFill::Filled, style(Red), error)
        && error == RenderError::InvalidGeometry, "Negative radius accepted.");
    require(!renderer.drawCircle({ {}, 10 }, ShapeFill::Filled, style(Red), error, 2)
        && error == RenderError::InvalidGeometry, "Invalid circle subdivision accepted.");
    require(!renderer.drawRectangle({ 10, 10, -1, 20 }, ShapeFill::Filled, style(Red), error)
        && error == RenderError::InvalidGeometry, "Negative rectangle accepted.");
    DrawOptions options = style(Red);
    options.lineWidth = 0;
    require(!renderer.drawLine({}, { 50, 50 }, options, error) && error == RenderError::InvalidOptions,
        "Zero line width accepted.");
    options = style(Red);
    options.transform.m02 = std::numeric_limits<float>::infinity();
    require(!renderer.drawRectangle({ 0, 0, 50, 50 }, ShapeFill::Filled, options, error)
        && error == RenderError::InvalidOptions, "Invalid transform accepted.");
    require(!renderer.setClip(Viewport{ 0, 0, -1, 10 }, error) && error == RenderError::InvalidClip,
        "Invalid clip accepted.");
    pixel(20, 20, Black);
    // Ошибки не меняют вид и отсечение; корректная команда продолжает кадр.
    const std::vector<Vector2> closed = { { 10, 10 }, { 10, 10 }, { 30, 10 }, { 60, 10 },
        { 60, 60 }, { 10, 60 }, { 10, 10 } };
    check(renderer.drawPolygon(closed, ShapeFill::Filled, style(Green), error), error);
    check(renderer.drawCircle({ { 100, 100 }, 0 }, ShapeFill::Filled, style(Red), error), error);
    check(renderer.drawRectangle({ 100, 100, 0, 10 }, ShapeFill::Filled, style(Red), error), error);
    check(renderer.endFrame(error), error);
    pixel(20, 20, Green);
    pixel(50, 50, Green);
    pixel(100, 100, Black);
    require(!renderer.drawPoint({}, style(Red), error) && error == RenderError::InvalidState,
        "Drawing after end accepted.");
    check(renderer.beginFrame(ImageSize, ImageSize, Blue, error), error);
    check(renderer.endFrame(error), error);
    pixel(20, 20, Blue);
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
    check(renderer.beginFrame(ImageSize, ImageSize, Blue, error), error);
    check(renderer.beginUi(screen(), error), error);
    check(renderer.drawRectangle({ 10, 10, 80, 80 }, ShapeFill::Filled, style(Red), error), error);
    check(renderer.drawRectangle({ 10, 10, 80, 80 }, ShapeFill::Filled, style({ 0, 1, 0, 0.5F }), error), error);
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
        check(renderer.beginFrame(ImageSize, ImageSize, Black, error), error);
        check(renderer.beginWorld(screenWorld(), error), error);
        for (int step = 0; step < 2; ++step) {
            const bool first = (step == 0) != reversed;
            // Слой имеет приоритет над ключом внутри слоя.
            check(renderer.drawRectangle({ 10, 10, 80, 80 }, ShapeFill::Filled,
                ordered(first ? Red : Blue, first ? -2 : -1, first ? 100 : -100), error), error);
            // Равные ключи сохраняют порядок подачи команд.
            check(renderer.drawRectangle({ 110, 10, 50, 50 }, ShapeFill::Filled,
                style(first ? Red : Green), error), error);
            // Сортировка внутри одного слоя, включая отрицательные значения.
            check(renderer.drawRectangle({ 200, 10, 40, 40 }, ShapeFill::Filled,
                ordered(first ? Red : Blue, 4, first ? 50 : -20), error), error);
            // Прозрачный фон должен оказаться позади непрозрачного объекта.
            check(renderer.drawRectangle({ 10, 110, 80, 80 }, ShapeFill::Filled,
                ordered(first ? Color{ 0, 0, 1, 0.5F } : Green, first ? -1 : 1), error), error);
            // Прозрачный передний слой смешивается с непрозрачным фоном.
            check(renderer.drawRectangle({ 110, 110, 80, 80 }, ShapeFill::Filled,
                ordered(first ? Red : Color{ 0, 0, 1, 0.5F }, first ? -1 : 1), error), error);
        }
        check(renderer.drawRectangle({ 200, 110, 40, 40 }, ShapeFill::Filled,
            style({ 1, 0, 0, 0.5F }), error), error);
        check(renderer.drawRectangle({ 200, 110, 40, 40 }, ShapeFill::Filled,
            style({ 0, 0, 1, 0.5F }), error), error);
        check(renderer.beginUi(screen(), error), error);
        check(renderer.drawRectangle({ 25, 25, 10, 10 }, ShapeFill::Filled,
            ordered(White, std::numeric_limits<int>::min()), error), error);
        check(renderer.endFrame(error), error);
        pixel(20, 20, Blue);
        pixel(30, 30, White);
        pixel(120, 20, reversed ? Red : Green);
        pixel(210, 20, Red);
        pixel(20, 120, Green);
        pixel(120, 120, { 0.5F, 0, 0.5F });
        pixel(210, 120, { 0.25F, 0, 0.5F });
    }

    check(renderer.beginFrame(ImageSize, ImageSize, Black, error), error);
    check(renderer.beginWorld(screenWorld(), error), error);
    check(renderer.setClip(Viewport{ 0, 0, 40, 100 }, error), error);
    check(renderer.drawRectangle({ 0, 0, 100, 100 }, ShapeFill::Filled, ordered(Red, 2), error), error);
    check(renderer.setClip(Viewport{ 60, 0, 40, 100 }, error), error);
    check(renderer.drawRectangle({ 0, 0, 100, 100 }, ShapeFill::Filled, ordered(Blue, 1), error), error);
    check(renderer.setClip(std::nullopt, error), error);
    // Отложенные команды владеют копией вершин и параметров.
    std::vector<Vector2> vertices = { { 10, 120 }, { 70, 120 }, { 10, 180 } };
    DrawOptions options = style(Blue);
    check(renderer.drawPolygon(vertices, ShapeFill::Filled, options, error), error);
    vertices.clear();
    options.color = Green;
    options.transform.m02 = 1000;
    for (float invalid : { std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() }) {
        options.order = invalid;
        require(!renderer.drawRectangle({ 0, 0, 100, 100 }, ShapeFill::Filled, options, error)
            && error == RenderError::InvalidOptions, "Nonfinite sort key accepted.");
    }
    check(renderer.endFrame(error), error);
    pixel(20, 20, Red);
    pixel(50, 20, Black);
    pixel(80, 20, Blue);
    pixel(20, 130, Blue);

    // Границы видов старше ключей сортировки; UI всегда идёт после мира.
    check(renderer.beginFrame(ImageSize, ImageSize, Black, error), error);
    check(renderer.beginWorld(screenWorld(), error), error);
    check(renderer.drawRectangle({ 0, 0, 100, 100 }, ShapeFill::Filled,
        ordered(Red, std::numeric_limits<int>::max()), error), error);
    check(renderer.beginWorld(screenWorld(), error), error);
    check(renderer.drawRectangle({ 0, 0, 100, 100 }, ShapeFill::Filled,
        ordered(Blue, std::numeric_limits<int>::min()), error), error);
    check(renderer.beginUi(screen(), error), error);
    check(renderer.drawRectangle({ 10, 10, 20, 20 }, ShapeFill::Filled,
        ordered(White, std::numeric_limits<int>::min()), error), error);
    check(renderer.drawRectangle({ 110, 10, 40, 40 }, ShapeFill::Filled, ordered(Red, 10), error), error);
    check(renderer.drawRectangle({ 110, 10, 40, 40 }, ShapeFill::Filled, ordered(Green, 1), error), error);
    check(renderer.endFrame(error), error);
    pixel(80, 80, Blue);
    pixel(20, 20, White);
    pixel(120, 20, Red);

    // Отмена кадра не переносит его команды в следующий запуск рендерера.
    check(renderer.beginFrame(ImageSize, ImageSize, Black, error), error);
    check(renderer.beginWorld(screenWorld(), error), error);
    check(renderer.drawRectangle({ 10, 10, 80, 80 }, ShapeFill::Filled, style(Red), error), error);
    renderer.shutdown();
    check(renderer.initialize(error), error);
    check(renderer.beginFrame(ImageSize, ImageSize, Black, error), error);
    check(renderer.beginUi(screen(), error), error);
    check(renderer.drawRectangle({ 10, 10, 30, 30 }, ShapeFill::Filled, style(Blue), error), error);
    check(renderer.endFrame(error), error);
    pixel(20, 20, Blue);
    pixel(80, 80, Black);
    renderer.shutdown();
}

int runTest(const std::string& mode)
{
    FramebufferApi api;
    if (!api.load()) {
        std::cout << "Pixel checks require optional FBO support from the test driver.\n";
        return 77;
    }
    RenderTarget target(api);
    target.initialize();
    if (mode == "primitives") { primitives(); }
    else if (mode == "gradient") { gradient(); }
    else if (mode == "transforms") { transforms(); }
    else if (mode == "views") { views(); }
    else if (mode == "errors") { errors(); }
    else if (mode == "state") { state(); }
    else if (mode == "sorting") { sorting(); }
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
