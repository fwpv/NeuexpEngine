#include "renderer.h"
#include "platform.h"
#include "platform_test_diagnostics.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using namespace Engine;
constexpr int WindowSize = 256;
constexpr Color Black = { 0, 0, 0 };
constexpr Color Red = { 1, 0, 0 };
constexpr Color Green = { 0, 1, 0 };
constexpr Color Blue = { 0, 0, 1 };
constexpr Color White = { 1, 1, 1 };

void require(bool success, const std::string& message)
{
    if (!success) { throw std::runtime_error(message); }
}

void check(bool success, const ResourceError& error)
{
    require(success && error.code == ResourceErrorCode::None, error.message);
}

void check(bool success, const RenderError& error)
{
    require(success && error == RenderError::None, "RenderError=" + std::to_string(static_cast<int>(error)));
    require(glGetError() == GL_NO_ERROR, "Unexpected OpenGL error.");
}

void pixel(int x, int y, Color expected, int tolerance = 2)
{
    std::array<GLubyte, 3> value{};
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, WindowSize - 1 - y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, value.data());
    const std::array<float, 3> target = { expected.red, expected.green, expected.blue };
    for (std::size_t index = 0; index < 3; ++index) {
        const int wanted = static_cast<int>(std::lround(target[index] * 255));
        require(std::abs(static_cast<int>(value[index]) - wanted) <= tolerance, "Pixel " + std::to_string(x) + "," + std::to_string(y) + " channel " + std::to_string(index)
            + " expected " + std::to_string(wanted) + " got " + std::to_string(value[index]));
    }
    require(glGetError() == GL_NO_ERROR, "Cannot read framebuffer pixels.");
}

UiView view()
{
    UiView result;
    UiError error;
    require(UiCanvas{}.createView({ 0, 0, WindowSize, WindowSize }, result, error), "Cannot create UI view.");
    return result;
}

void frame(Renderer2D& renderer, Color background = Black)
{
    RenderError error;
    check(renderer.beginFrame(WindowSize, WindowSize, background, error), error);
    check(renderer.beginUi(view(), error), error);
}

Texture create(Renderer2D& renderer, const Image& image, bool keep = false)
{
    Texture result;
    ResourceError error;
    check(renderer.createTexture(image, result, error, keep), error);
    return result;
}

TextureSampling nearest()
{
    TextureSampling result;
    result.filter = TextureFilter::Nearest;
    return result;
}

void pixels(Renderer2D& renderer, const ResourceFiles& files)
{
    ResourceError resourceError;
    RenderError error;
    std::vector<std::uint8_t> raw;
    check(files.readBinary("raw_3x2.rgba", raw, resourceError), resourceError);
    const auto texture = create(renderer, { 3, 2, raw }, true);
    require(texture.width() == 3 && texture.height() == 2 && texture.image()->pixels == raw, "Original CPU pixels or dimensions changed.");
    frame(renderer);
    check(renderer.drawTexture(texture, { 0, 0, 120, 80 }, {}, error, nearest()), error);
    check(renderer.endFrame(error), error);
    pixel(10, 10, Red);
    pixel(60, 10, Green);
    pixel(110, 10, Blue);
    pixel(10, 60, White);
    pixel(60, 60, Black);
    pixel(110, 60, { 128.0F / 255, 128.0F / 255, 0 });
    Texture png;
    check(renderer.loadTexture(files, "rgba_2x2.png", png, resourceError), resourceError);
    frame(renderer, Blue);
    check(renderer.drawTexture(png, { 0, 0, 80, 80 }, {}, error, nearest()), error);
    check(renderer.endFrame(error), error);
    pixel(10, 10, Blue);
    pixel(60, 10, { 0, 128.0F / 255, 127.0F / 255 });
    pixel(10, 60, Blue);
    pixel(60, 60, { 64.0F / 255, 64.0F / 255, 1 });
}

void sampling(Renderer2D& renderer)
{
    RenderError error;
    const auto texture = create(renderer, { 2, 1, { 255, 0, 0, 255, 0, 0, 255, 255 } });
    frame(renderer);
    check(renderer.drawTexture(texture, { 0, 0, 100, 40 }, {}, error, nearest()), error);
    check(renderer.drawTexture(texture, { 0, 50, 100, 40 }, {}, error), error);
    TextureSampling repeated = nearest();
    repeated.wrapX = TextureWrap::Repeat;
    repeated.repeats = { 2, 1 };
    check(renderer.drawTexture(texture, { 0, 100, 100, 40 }, {}, error, repeated), error);
    repeated.wrapX = TextureWrap::Clamp;
    check(renderer.drawTexture(texture, { 0, 150, 100, 40 }, {}, error, repeated), error);
    check(renderer.endFrame(error), error);
    pixel(49, 10, Red);
    pixel(50, 10, Blue);
    pixel(0, 60, Red);
    pixel(99, 60, Blue);
    pixel(49, 60, { 0.51F, 0, 0.49F }, 3);
    pixel(10, 110, Red);
    pixel(35, 110, Blue);
    pixel(60, 110, Red);
    pixel(85, 110, Blue);
    pixel(10, 160, Red);
    pixel(35, 160, Blue);
    pixel(60, 160, Blue);
    pixel(99, 160, Blue);
    // Загрузка нейтрализует внешние настройки передачи пикселей и восстанавливает их.
    glPixelTransferf(GL_RED_SCALE, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 17);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 3);
    const auto uploaded = create(renderer, { 1, 1, { 255, 0, 0, 255 } });
    GLint rowLength = 0;
    GLfloat redScale = 1;
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
    glGetFloatv(GL_RED_SCALE, &redScale);
    require(rowLength == 17 && redScale == 0, "Upload did not restore pixel transfer state.");
    glPixelTransferf(GL_RED_SCALE, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    frame(renderer);
    check(renderer.drawTexture(uploaded, { 0, 0, 30, 30 }, {}, error), error);
    check(renderer.endFrame(error), error);
    pixel(10, 10, Red);
    const auto vertical = create(renderer, { 1, 2, { 0, 255, 0, 255, 255, 0, 0, 255 } });
    frame(renderer);
    repeated.wrapY = TextureWrap::Repeat;
    repeated.repeats = { 1, 2 };
    check(renderer.drawTexture(vertical, { 0, 0, 40, 100 }, {}, error, repeated), error);
    check(renderer.endFrame(error), error);
    pixel(10, 10, Green);
    pixel(10, 35, Red);
    pixel(10, 60, Green);
    pixel(10, 85, Red);
}

void integration(Renderer2D& renderer)
{
    RenderError error;
    auto texture = create(renderer, { 1, 1, { 255, 0, 0, 255 } });
    frame(renderer);
    DrawOptions front;
    front.color = { 0, 0, 1, 0.5F };
    front.layer = 2;
    check(renderer.drawRectangle({ 0, 0, 80, 80 }, ShapeFill::Filled, front, error), error);
    DrawOptions behind;
    behind.layer = -2;
    check(renderer.setClip(Viewport{ 0, 0, 40, 80 }, error), error);
    check(renderer.drawTexture(texture, { 0, 0, 80, 80 }, behind, error), error);
    check(renderer.setClip(std::nullopt, error), error);
    DrawOptions transform;
    transform.transform = Transform2{ { 130, 30 }, Pi / 2, { 1, 1 }, {} }.toAffine();
    transform.color = { 0.5F, 1, 1, 1 };
    check(renderer.drawTexture(texture, { 0, 0, 20, 40 }, transform, error), error);
    check(renderer.endFrame(error), error);
    pixel(10, 10, { 0.5F, 0, 0.5F });
    pixel(60, 10, { 0, 0, 0.5F });
    pixel(100, 40, { 0.5F, 0, 0 });
    // Команда удерживает ресурс после освобождения последней пользовательской ссылки.
    frame(renderer);
    check(renderer.drawTexture(texture, { 0, 0, 80, 80 }, {}, error), error);
    texture = {};
    renderer.collectUnusedTextures();
    require(renderer.textureStats().count == 1, "Queued texture was collected prematurely.");
    check(renderer.endFrame(error), error);
    pixel(10, 10, Red);
    GLint name = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &name);
    require(name != 0 && glIsTexture(static_cast<GLuint>(name)), "Expected a live GPU texture.");
    renderer.collectUnusedTextures();
    require(renderer.textureStats().count == 0 && !glIsTexture(static_cast<GLuint>(name)), "Unused GPU texture leaked.");
    texture = create(renderer, { 1, 1, { 0, 255, 0, 255 } });
    check(renderer.beginFrame(WindowSize, WindowSize, Black, error), error);
    Camera2 camera;
    CameraView world;
    CameraError cameraError;
    require(camera.createView({ 0, 0, WindowSize, WindowSize }, world, cameraError), "Camera failed.");
    check(renderer.beginWorld(world, error), error);
    check(renderer.drawTexture(texture, { -128, -128, 256, 256 }, {}, error), error);
    check(renderer.beginUi(view(), error), error);
    DrawOptions ui;
    ui.color = Red;
    ui.layer = -100;
    check(renderer.drawRectangle({ 0, 0, 30, 30 }, ShapeFill::Filled, ui, error), error);
    check(renderer.endFrame(error), error);
    pixel(10, 10, Red);
    pixel(100, 100, Green);
}

void resources(Renderer2D& renderer, const ResourceFiles& files)
{
    ResourceError error;
    RenderError renderError;
    Texture first;
    Texture second;
    check(renderer.loadTexture(files, "rgba_2x2.png", first, error, true), error);
    check(renderer.loadTexture(files, "./rgba_2x2.png", second, error), error);
    require(first == second && renderer.textureStats().count == 1
        && renderer.textureStats().retainedPixelBytes == 16, "Cache duplicated texture or lost pixels.");
    const auto bytes = renderer.textureStats().gpuBytes;
    for (int index = 0; index < 50; ++index) {
        check(renderer.loadTexture(files, "rgba_2x2.png", second, error), error);
    }
    require(renderer.textureStats().gpuBytes == bytes, "Repeated loads consumed video memory.");
    require(!renderer.loadTexture(files, "truncated.png", second, error) && first == second
        && renderer.textureStats().count == 1, "Failed load modified cache or output.");
    require(!renderer.createTexture({ 1, 1, {} }, second, error) && first == second, "Invalid upload changed output.");
    require(!renderer.setTextureBudget(1, error), "Live renderer accepted a budget change.");
    frame(renderer);
    require(!renderer.drawTexture({}, { 0, 0, 10, 10 }, {}, renderError)
        && renderError == RenderError::InvalidTexture, "Empty texture accepted.");
    TextureSampling invalid;
    invalid.repeats.x = std::numeric_limits<float>::quiet_NaN();
    require(!renderer.drawTexture(first, { 0, 0, 10, 10 }, {}, renderError, invalid), "NaN repetition accepted.");
    check(renderer.drawTexture(first, { 0, 0, 10, 10 }, {}, renderError), renderError);
    check(renderer.endFrame(renderError), renderError);
    GLint liveName = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &liveName);
    Renderer2D other;
    check(other.initialize(renderError), renderError);
    frame(other);
    require(!other.drawTexture(first, { 0, 0, 10, 10 }, {}, renderError)
        && renderError == RenderError::InvalidTexture, "Foreign texture accepted.");
    other.shutdown();
    renderer.shutdown();
    require(!glIsTexture(static_cast<GLuint>(liveName)), "Shutdown did not delete the GPU object.");
    require(!first.isValid() && !second.isValid() && !first.image() && first.width() == 0, "Shutdown left usable texture handles.");
    check(renderer.initialize(renderError), renderError);
    frame(renderer);
    require(!renderer.drawTexture(first, { 0, 0, 10, 10 }, {}, renderError), "Old texture accepted after restart.");
    check(renderer.endFrame(renderError), renderError);
    first = {};
    second = {};
    check(renderer.loadTexture(files, "rgba_2x2.png", first, error), error);
    require(!first.image(), "CPU pixels were retained without request.");
    require(!renderer.loadTexture(files, "rgba_2x2.png", second, error, true)
        && error.code == ResourceErrorCode::InvalidState, "Cache silently changed pixel retention policy.");
    first = {};
    renderer.collectUnusedTextures();
    check(renderer.loadTexture(files, "rgba_2x2.png", first, error, true), error);
    require(first.image(), "Reload did not retain pixels.");
    renderer.shutdown();
    check(renderer.setTextureBudget(1, error), error);
    check(renderer.initialize(renderError), renderError);
    require(!renderer.createTexture({ 1, 1, { 255, 0, 0, 255 } }, first, error)
        && error.code == ResourceErrorCode::LimitExceeded && renderer.textureStats().count == 0, "Texture budget ignored.");
    renderer.shutdown();
    check(renderer.setTextureBudget(4, error), error);
    check(renderer.initialize(renderError), renderError);
    first = create(renderer, { 1, 1, { 255, 0, 0, 255 } });
    require(!renderer.createTexture({ 1, 1, { 0, 255, 0, 255 } }, second, error)
        && error.code == ResourceErrorCode::LimitExceeded, "Aggregate texture budget ignored.");
    first = {};
    second = create(renderer, { 1, 1, { 0, 255, 0, 255 } });
    require(renderer.textureStats().count == 1, "Unused texture was not collected before upload.");
}
}

int main(int argc, char** argv)
{
#ifndef _WIN32
    if (!std::getenv("DISPLAY") || !*std::getenv("DISPLAY")) { return 77; }
#endif
    int result = 1;
    Renderer2D renderer;
    try {
        require(argc == 2, "Expected texture test mode.");
        Platform::WindowOptions options;
        options.width = WindowSize;
        options.height = WindowSize;
        options.title = "Texture checks";
        std::vector<Platform::Error> errors;
        const bool initialized = Platform::initialize(options, errors);
        PlatformTest::reportErrors(errors);
        require(initialized, "Cannot initialize test window.");
        RenderError renderError;
        check(renderer.initialize(renderError), renderError);
        ResourceFiles files;
        ResourceError error;
        check(files.initialize(error), error);
        const std::string mode = argv[1];
        if (mode == "pixels") { pixels(renderer, files); }
        else if (mode == "sampling") { sampling(renderer); }
        else if (mode == "integration") { integration(renderer); }
        else if (mode == "resources") { resources(renderer, files); }
        else { throw std::runtime_error("Unknown mode."); }
        result = 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
    }
    renderer.shutdown();
    std::vector<Platform::Error> errors;
    Platform::shutdown(errors);
    PlatformTest::reportErrors(errors);
    return errors.empty() ? result : 1;
}
