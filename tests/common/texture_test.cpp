#include "atlas.h"
#include "renderer.h"
#include "platform.h"
#include "platform_test_diagnostics.h"
#include "render_target.h"

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
    renderer.beginFrame(WindowSize, WindowSize, background);
    renderer.setUiView(view());
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
    renderer.drawTexture(texture, { 0, 0, 120, 80 }, {}, nearest());
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
    renderer.drawTexture(png, { 0, 0, 80, 80 }, {}, nearest());
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
    renderer.drawTexture(texture, { 0, 0, 100, 40 }, {}, nearest());
    renderer.drawTexture(texture, { 0, 50, 100, 40 }, {});
    TextureSampling repeated = nearest();
    repeated.wrapX = TextureWrap::Repeat;
    repeated.repeats = { 2, 1 };
    renderer.drawTexture(texture, { 0, 100, 100, 40 }, {}, repeated);
    repeated.wrapX = TextureWrap::Clamp;
    renderer.drawTexture(texture, { 0, 150, 100, 40 }, {}, repeated);
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
    renderer.drawTexture(uploaded, { 0, 0, 30, 30 }, {});
    check(renderer.endFrame(error), error);
    pixel(10, 10, Red);
    const auto vertical = create(renderer, { 1, 2, { 0, 255, 0, 255, 255, 0, 0, 255 } });
    frame(renderer);
    repeated.wrapY = TextureWrap::Repeat;
    repeated.repeats = { 1, 2 };
    renderer.drawTexture(vertical, { 0, 0, 40, 100 }, {}, repeated);
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
    renderer.drawRectangle({ 0, 0, 80, 80 }, front, ShapeFill::Filled);
    DrawOptions behind;
    behind.layer = -2;
    renderer.setClip(Viewport{ 0, 0, 40, 80 });
    renderer.drawTexture(texture, { 0, 0, 80, 80 }, behind);
    renderer.setClip(std::nullopt);
    DrawOptions transform;
    transform.transform = Transform2{ { 130, 30 }, Pi / 2, { 1, 1 }, {} }.toAffine();
    transform.color = { 0.5F, 1, 1, 1 };
    renderer.drawTexture(texture, { 0, 0, 20, 40 }, transform);
    check(renderer.endFrame(error), error);
    pixel(10, 10, { 0.5F, 0, 0.5F });
    pixel(60, 10, { 0, 0, 0.5F });
    pixel(100, 40, { 0.5F, 0, 0 });
    // Команда удерживает ресурс после освобождения последней пользовательской ссылки.
    frame(renderer);
    renderer.drawTexture(texture, { 0, 0, 80, 80 }, {});
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
    renderer.beginFrame(WindowSize, WindowSize, Black);
    Camera2 camera;
    CameraView world;
    CameraError cameraError;
    require(camera.createView({ 0, 0, WindowSize, WindowSize }, world, cameraError), "Camera failed.");
    renderer.setWorldView(world);
    renderer.drawTexture(texture, { -128, -128, 256, 256 }, {});
    renderer.setUiView(view());
    DrawOptions ui;
    ui.color = Red;
    ui.layer = -100;
    renderer.drawRectangle({ 0, 0, 30, 30 }, ui, ShapeFill::Filled);
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
    renderer.drawTexture({}, { 0, 0, 10, 10 });
    require(!renderer.endFrame(renderError) && renderError == RenderError::InvalidTexture,
        "Empty texture accepted.");
    TextureSampling invalid;
    invalid.repeats.x = std::numeric_limits<float>::quiet_NaN();
    frame(renderer);
    renderer.drawTexture(first, { 0, 0, 10, 10 }, {}, invalid);
    require(!renderer.endFrame(renderError) && renderError == RenderError::InvalidOptions,
        "NaN repetition accepted.");
    frame(renderer);
    renderer.drawTexture(first, { 0, 0, 10, 10 }, {});
    check(renderer.endFrame(renderError), renderError);
    GLint liveName = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &liveName);
    Renderer2D other;
    check(other.initialize(renderError), renderError);
    frame(other);
    other.drawTexture(first, { 0, 0, 10, 10 });
    require(!other.endFrame(renderError) && renderError == RenderError::InvalidTexture,
        "Foreign texture accepted.");
    other.shutdown();
    renderer.shutdown();
    require(!glIsTexture(static_cast<GLuint>(liveName)), "Shutdown did not delete the GPU object.");
    require(!first.isValid() && !second.isValid() && !first.image() && first.width() == 0, "Shutdown left usable texture handles.");
    check(renderer.initialize(renderError), renderError);
    frame(renderer);
    renderer.drawTexture(first, { 0, 0, 10, 10 });
    require(!renderer.endFrame(renderError) && renderError == RenderError::InvalidTexture,
        "Old texture accepted after restart.");
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

void sprites(Renderer2D& renderer)
{
    RenderError error;
    const auto texture = create(renderer, { 2, 2, {
        255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 255, 255, 255, 255 } });
    Sprite whole(texture);
    require(whole.size == Size2{2, 2} && !whole.textureRect, "Wrong whole sprite defaults.");
    whole.size = {80, 80};
    whole.filter = TextureFilter::Nearest;
    frame(renderer);
    renderer.drawSprite(whole);
    Sprite region(texture, {1, 0, 1, 1});
    require(region.size == Size2{1, 1}, "Wrong region sprite defaults.");
    region.size = {80, 40};
    region.transform.position = {100, 0};
    renderer.drawSprite(region);
    // Изменение области не меняет размер; уже принятая команда хранит снимок состояния.
    region.textureRect = Rect{0, 1, 1, 1};
    region.transform.position.y = 50;
    renderer.drawSprite(region);
    region.visible = false;
    region.transform.position.y = 100;
    renderer.drawSprite(region);
    check(renderer.endFrame(error), error);
    pixel(10, 10, Red);
    pixel(70, 10, Green);
    pixel(10, 70, Blue);
    pixel(70, 70, White);
    pixel(100, 0, Green);
    pixel(179, 39, Green);
    pixel(140, 70, Blue);
    pixel(140, 120, Black);

    frame(renderer);
    region.visible = true;
    region.size = {20, 10};
    region.transform = {{100, 100}, Pi / 2, {2, 3}, {10, 5}};
    renderer.drawSprite(region);
    // Отрицательный масштаб отражает текстуру относительно origin.
    whole.transform = {{240, 0}, 0, {-1, 1}, {}};
    renderer.drawSprite(whole);
    region.transform.scale = {0, 1};
    region.transform.position = {30, 150};
    renderer.drawSprite(region);
    check(renderer.endFrame(error), error);
    pixel(90, 85, Blue);
    pixel(110, 115, Blue);
    pixel(80, 100, Black);
    pixel(100, 75, Black);
    pixel(170, 10, Green);
    pixel(230, 10, Red);
    pixel(30, 150, Black);

    frame(renderer);
    region.transform = {{100, 100}, Pi / 2, {2, 3}, {10, 5}};
    renderer.drawSprite(region);
    DrawOptions outline;
    outline.color = White;
    outline.lineWidth = 3;
    renderer.drawPolygon(region.polygon(), outline, ShapeFill::Outline);
    check(renderer.endFrame(error), error);
    pixel(100, 100, Blue);
    pixel(85, 100, White);
    pixel(100, 80, White);
    pixel(80, 100, Black);

    // Цвет и альфа текстуры, спрайта и DrawOptions перемножаются.
    const auto translucent = create(renderer, {1, 1, {255, 255, 255, 128}});
    Sprite tinted(translucent);
    tinted.size = {40, 40};
    tinted.color = {0.5F, 1, 0.25F, 0.5F};
    tinted.transform.position = {10, 10};
    DrawOptions parent;
    parent.transform = Transform2{{40, 40}, 0, {2, 2}, {}}.toAffine();
    parent.color = {1, 0.5F, 1, 0.5F};
    frame(renderer);
    renderer.drawSprite(tinted, parent);
    check(renderer.endFrame(error), error);
    const float alpha = (128.0F / 255) * 0.25F;
    pixel(70, 70, {0.5F * alpha, 0.5F * alpha, 0.25F * alpha});
    pixel(50, 50, Black);

    // Неполная область больше одного текселя: интерполяция внутри, без соседей снаружи.
    const auto strip = create(renderer, {4, 1, {
        0, 255, 0, 255, 255, 0, 0, 255, 0, 0, 255, 255, 0, 255, 0, 255}});
    Sprite cropped(strip, {1, 0, 2, 1});
    cropped.size = {100, 40};
    frame(renderer);
    renderer.drawSprite(cropped);
    check(renderer.endFrame(error), error);
    pixel(0, 10, Red);
    pixel(99, 10, Blue);
    pixel(49, 10, {0.51F, 0, 0.49F}, 3);
}

void atlasChecks(Renderer2D& renderer)
{
    ResourceError error;
    Texture texture;
    check(renderer.createTexture(Image{2, 1, {255,255,255,0, 255,255,255,255}}, texture, error, true), error);
    TextureAtlas atlas;
    check(atlas.load(texture, "# regions\r\nclear 0 0 1 1\nsolid 1 0 1 1\n", error), error);
    Sprite sprite;
    require(atlas.createSprite("solid", sprite), "Missing region.");
    require(sprite.texture == texture && sprite.size == Size2{1, 1}, "Atlas did not share texture or set size.");
    sprite.size = {20, 10};
    sprite.transform = {Vector2{50, 60}, 0.7F, Vector2{-2, 3}, Vector2{10, 5}};
    const auto center = sprite.transform.toAffine().transformPoint(Vector2{10, 5});
    require(sprite.hitTest(center, true), "Transformed alpha hit missed.");
    require(!sprite.hitTest(sprite.transform.toAffine().transformPoint(Vector2{21, 5})), "Outside hit accepted.");
    sprite.color.alpha = 0.25F;
    require(sprite.hitTest(center, true, 0.2F) && !sprite.hitTest(center, true, 0.3F), "Alpha modulation ignored.");
    sprite.visible = false;
    require(!sprite.hitTest(center), "Invisible hit accepted.");
    require(atlas.createSprite("clear", sprite), "Missing transparent region.");
    require(sprite.hitTest(Vector2{0.5F, 0.5F}) && !sprite.hitTest(Vector2{0.5F, 0.5F}, true), "Transparency ignored.");
    require(!sprite.hitTest(Vector2{1, 0.5F}), "Right boundary included.");
    require(!atlas.load(texture, "solid 0 0 1 1\nsolid 1 0 1 1", error), "Duplicate region accepted.");
    require(atlas.find("clear") != nullptr, "Failed parse changed atlas.");
    require(!atlas.load(texture, "outside 2 0 1 1", error), "Outside region accepted.");
    require(!atlas.createSprite("missing", sprite), "Missing region accepted.");
    Texture discarded;
    check(renderer.createTexture(Image{1, 1, {255,255,255,255}}, discarded, error), error);
    Sprite noPixels(discarded);
    require(noPixels.hitTest(Vector2{0.5F, 0.5F}) && !noPixels.hitTest(Vector2{0.5F, 0.5F}, true), "Missing pixel contract failed.");
}
void spriteErrors(Renderer2D& renderer)
{
    RenderError error;
    const auto texture = create(renderer, {1, 1, {255, 255, 255, 255}});
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Sprite sprite(texture);
    const std::array<Rect, 6> invalid = {{
        {-1, 0, 1, 1}, {0, 0, 2, 1}, {0, 1, 1, 1},
        {0, 0, -1, 1}, {nan, 0, 1, 1}, {0, 0, 1, nan}}};
    for (const auto& area : invalid) {
        sprite.textureRect = area;
        frame(renderer);
        renderer.drawSprite(sprite);
        require(!renderer.endFrame(error) && error == RenderError::InvalidGeometry, "Invalid sprite region accepted.");
    }
    sprite.textureRect.reset();
    sprite.size.width = -1;
    frame(renderer);
    renderer.drawSprite(sprite);
    require(!renderer.endFrame(error) && error == RenderError::InvalidGeometry, "Negative sprite size accepted.");
    sprite.size = {20, 20};
    sprite.transform.rotation = nan;
    frame(renderer);
    renderer.drawSprite(sprite);
    require(!renderer.endFrame(error) && error == RenderError::InvalidOptions, "NaN sprite rotation accepted.");
    sprite.transform = {};
    sprite.color.alpha = 2;
    frame(renderer);
    renderer.drawSprite(sprite);
    require(!renderer.endFrame(error) && error == RenderError::InvalidOptions, "Invalid sprite color accepted.");
    sprite = {};
    frame(renderer);
    renderer.drawSprite(sprite);
    require(!renderer.endFrame(error) && error == RenderError::InvalidTexture, "Empty sprite texture accepted.");
    sprite.visible = false;
    frame(renderer);
    renderer.drawSprite(sprite);
    check(renderer.endFrame(error), error);
    sprite = Sprite(texture, {1, 1, 0, 0});
    frame(renderer);
    renderer.drawSprite(sprite);
    check(renderer.endFrame(error), error);
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
        // Оконный буфер намеренно меньше тестового FBO размером 256x256.
        options.width = 64;
        options.height = 64;
        options.title = "Texture checks";
        Platform::Error failure;
        const bool initialized = Platform::initialize(options, failure);
        PlatformTest::reportError(failure);
        require(initialized, "Cannot initialize test window.");
        RenderError renderError;
        check(renderer.initialize(renderError), renderError);
        ResourceFiles files;
        ResourceError error;
        check(files.initialize(error), error);
        const std::string mode = argv[1];
        if (mode == "resources" || mode == "sprite_errors" || mode == "atlas") {
            if (mode == "resources") { resources(renderer, files); }
            else if (mode == "atlas") { atlasChecks(renderer); }
            else { spriteErrors(renderer); }
            result = 0;
        } else {
            require(mode == "pixels" || mode == "sampling" || mode == "integration" || mode == "sprites", "Unknown mode.");
            RenderTest::FramebufferApi api;
            if (!api.load()) {
                std::cout << "Pixel checks require optional FBO support from the test driver.\n";
                result = 77;
            } else {
                // Скрытое окно проверяет независимость результата от оконного буфера.
#ifdef _WIN32
                const HWND window = WindowFromDC(wglGetCurrentDC());
                require(window != nullptr, "Cannot find the test window.");
                ShowWindow(window, SW_HIDE);
#else
                Display* display = glXGetCurrentDisplay();
                XUnmapWindow(display, glXGetCurrentDrawable());
                XSync(display, False);
#endif
                Platform::Error windowError;
                require(PlatformTest::checkOperation(Platform::processEvents(windowError), windowError),
                    "Cannot process test window events.");
                RenderTest::RenderTarget target(api, WindowSize);
                target.initialize();
                if (mode == "pixels") { pixels(renderer, files); }
                else if (mode == "sampling") { sampling(renderer); }
                else if (mode == "sprites") { sprites(renderer); }
                else { integration(renderer); }
                result = 0;
            }
        }
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
    }
    renderer.shutdown();
    Platform::Error failure;
    Platform::shutdown(failure);
    PlatformTest::reportError(failure);
    return failure.operation.empty() ? result : 1;
}
