#include "atlas.h"
#include "animation.h"
#include "renderer.h"
#include "platform.h"
#include "platform_test_diagnostics.h"
#include "render_target.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

#include <algorithm>
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

void animationChecks(Renderer2D& renderer)
{
    const auto texture = create(renderer, Image{3, 1, std::vector<std::uint8_t>{
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255}}, true);
    const std::vector<AnimationFrame> sequence{
        AnimationFrame{Rect{0, 0, 1, 1}, 0.125},
        AnimationFrame{Rect{1, 0, 1, 1}, 0.25},
        AnimationFrame{Rect{2, 0, 1, 1}, 0.125}};
    ResourceError failure;
    Animation animation;
    animation.play();
    animation.update(1);
    require(!animation.isPlaying() && !animation.isFinished(), "Empty animation started.");
    check(animation.load(texture, sequence, failure), failure);
    require(animation.currentFrame() == 0 && !animation.isPlaying(), "Load must stop on first frame.");
    animation.size = {60, 60};
    animation.transform.position = {20, 20};
    animation.color = Color{0.5F, 1, 1, 1};
    animation.filter = TextureFilter::Nearest;
    animation.play();
    animation.update(0.125);
    require(animation.currentFrame() == 1 && animation.elapsedTime() == 0.125, "Exact frame boundary failed.");
    require(animation.size == Size2{60, 60} && animation.transform.position == Vector2{20, 20}
        && animation.color.red == 0.5F && animation.texture == texture, "Frame change altered sprite properties.");
    require(animation.hitTest(Vector2{30, 30}, true), "Animated sprite hit test failed.");
    frame(renderer);
    renderer.drawSprite(animation);
    RenderError error;
    check(renderer.endFrame(error), error);
    pixel(30, 30, Green);
    require(animation.elapsedTime() == 0.125, "Rendering advanced animation.");

    animation.pause();
    animation.update(10);
    require(animation.elapsedTime() == 0.125 && !animation.isPlaying(), "Pause advanced time.");
    animation.play();
    animation.update(0.25);
    require(animation.currentFrame() == 2, "Resume lost elapsed time.");
    animation.update(0.125);
    require(animation.currentFrame() == 0 && animation.elapsedTime() == 0 && !animation.isFinished(), "Loop boundary failed.");
    animation.update(5000.25);
    require(animation.currentFrame() == 1 && animation.elapsedTime() == 0.25, "Multiple cycles failed.");
    animation.stop();
    require(animation.currentFrame() == 0 && animation.elapsedTime() == 0 && !animation.isPlaying(), "Stop failed.");

    animation.setMode(AnimationMode::Once);
    animation.restart();
    animation.update(0.375);
    require(animation.currentFrame() == 2 && !animation.isFinished(), "Last frame completed too early.");
    animation.update(0.125);
    require(animation.isFinished() && !animation.isPlaying() && animation.currentFrame() == 2
        && animation.elapsedTime() == 0.5, "Once completion failed.");
    animation.play();
    animation.update(10);
    require(animation.isFinished() && !animation.isPlaying() && animation.elapsedTime() == 0.5,
        "Completed animation restarted implicitly.");
    animation.restart();
    require(!animation.isFinished() && animation.isPlaying() && animation.currentFrame() == 0, "Restart failed.");
    animation.setSpeed(2);
    animation.update(0.0625);
    require(animation.currentFrame() == 1, "Playback speed ignored.");
    animation.setSpeed(0);
    animation.update(5);
    require(animation.elapsedTime() == 0.125 && animation.isPlaying(), "Zero speed failed.");
    animation.setSpeed(1);
    Animation independent = animation;
    independent.update(0.25);
    require(animation.currentFrame() == 1 && independent.currentFrame() == 2, "Instances share playback state.");

    for (const int fps : {30, 60, 120}) {
        Animation sample;
        check(sample.load(texture, sequence, failure), failure);
        sample.restart();
        for (int step = 0; step < fps * 2 + fps / 5; ++step) {
            sample.update(1.0 / fps);
        }
        require(sample.currentFrame() == 1 && std::abs(sample.elapsedTime() - 0.2) < 1e-12,
            "Playback depends on FPS.");
        sample.setMode(AnimationMode::Once);
        sample.restart();
        int completions = 0;
        for (int step = 0; step < fps * 2; ++step) {
            const bool wasFinished = sample.isFinished();
            sample.update(1.0 / fps);
            if (!wasFinished && sample.isFinished()) {
                ++completions;
            }
        }
        require(completions == 1 && sample.currentFrame() == 2, "Once must complete exactly once.");
    }

    Animation uniform;
    check(uniform.load(texture, std::vector<Rect>{Rect{0, 0, 1, 1}, Rect{1, 0, 1, 1}}, 0.125, failure), failure);
    uniform.restart();
    uniform.update(0.125);
    require(uniform.currentFrame() == 1, "Uniform frame duration failed.");
    check(uniform.load(texture, std::vector<Rect>{Rect{0, 0, 1, 1}}, 0.125, failure), failure);
    uniform.setMode(AnimationMode::Once);
    uniform.restart();
    uniform.update(100);
    require(uniform.isFinished() && uniform.currentFrame() == 0, "Single-frame completion failed.");

    Animation reverse;
    check(reverse.load(texture, sequence, failure), failure);
    require(reverse.frameCount() == 3, "Wrong animation frame count.");
    reverse.setFrame(1);
    require(reverse.currentFrame() == 1 && reverse.elapsedTime() == 0.125 && !reverse.isPlaying(), "Selecting paused frame failed.");
    reverse.play();
    reverse.update(0.0625);
    reverse.setDirection(AnimationDirection::Backward);
    require(reverse.currentFrame() == 1 && reverse.elapsedTime() == 0.1875, "Direction change lost current frame time.");
    reverse.update(0.1875);
    require(reverse.currentFrame() == 0, "Backward transition failed.");
    reverse.update(0.125);
    require(reverse.currentFrame() == 2 && reverse.elapsedTime() == 0, "Backward loop failed.");
    reverse.update(5000.125);
    require(reverse.currentFrame() == 1, "Backward multi-cycle update failed.");
    reverse.setMode(AnimationMode::Once);
    reverse.restart();
    require(reverse.currentFrame() == 2, "Backward restart did not select last frame.");
    reverse.update(0.5);
    require(reverse.isFinished() && reverse.currentFrame() == 0 && !reverse.isPlaying(), "Backward Once failed.");
    reverse.setDirection(AnimationDirection::Forward);
    require(!reverse.isFinished() && reverse.currentFrame() == 0 && !reverse.isPlaying(), "Reversing finished animation failed.");
    reverse.play();
    reverse.update(0.125);
    require(reverse.currentFrame() == 1, "Cannot play away from finished endpoint.");
    reverse.setFrame(2);
    require(reverse.isPlaying() && reverse.elapsedTime() == 0.375, "Selecting active frame failed.");
    reverse.update(0.125);
    require(reverse.isFinished(), "Selected final frame did not complete.");
    reverse.setFrame(0);
    require(!reverse.isFinished() && !reverse.isPlaying(), "Frame selection did not clear finished.");
    bool invalidIndex = false;
    try {
        reverse.setFrame(reverse.frameCount());
    } catch (const std::out_of_range&) {
        invalidIndex = true;
    }
    require(invalidIndex && reverse.currentFrame() == 0, "Invalid frame index changed state.");

    const std::vector<Rect> equalFrames{Rect{0, 0, 1, 1}, Rect{1, 0, 1, 1}, Rect{2, 0, 1, 1}};
    Animation held;
    check(held.load(texture, equalFrames, 0.125, failure), failure);
    held.setMode(AnimationMode::Clamp);
    held.setFrame(1);
    held.play();
    held.update(10.0625);
    require(held.currentFrame() == 2 && held.isPlaying() && !held.isFinished(), "Clamp must keep playing at endpoint.");
    held.setDirection(AnimationDirection::Backward);
    held.update(0.0625);
    require(held.currentFrame() == 1, "Clamp lost partial interval when reversing.");
    held.update(10.0625);
    require(held.currentFrame() == 0 && !held.isFinished(), "Backward Clamp failed.");
    held.setDirection(AnimationDirection::Forward);
    held.update(0.0625);
    require(held.currentFrame() == 1, "Backward endpoint lost partial interval.");
    held.pause();
    held.setFrame(held.currentFrame());
    held.play();
    held.update(0.0625);
    require(held.currentFrame() == 1, "Frame selection did not reset partial interval.");
    held.update(0.0625);
    require(held.currentFrame() == 2, "Frame duration after selection changed.");

    for (const auto direction : {AnimationDirection::Forward, AnimationDirection::Backward}) {
        for (const int fps : {30, 60, 120}) {
            Animation sample;
            check(sample.load(texture, sequence, failure), failure);
            sample.setDirection(direction);
            sample.restart();
            for (int step = 0; step < fps * 2 + fps / 5; ++step) {
                sample.update(1.0 / fps);
            }
            require(sample.currentFrame() == 1 && std::abs(sample.elapsedTime() - 0.2) < 1e-12,
                "Directional playback depends on FPS.");
        }
    }

    auto invalid = sequence;
    invalid[1].textureRect.size.width = 2;
    require(!animation.load(texture, invalid, failure), "Different frame sizes accepted.");
    invalid = sequence;
    invalid[1].duration = 0;
    require(!animation.load(texture, invalid, failure), "Zero duration accepted.");
    invalid[1].duration = std::numeric_limits<double>::infinity();
    require(!animation.load(texture, invalid, failure), "Infinite duration accepted.");
    invalid = sequence;
    invalid[1].textureRect.position.x = 3;
    require(!animation.load(texture, invalid, failure), "Out-of-texture frame accepted.");
    require(!animation.load(Texture{}, sequence, failure), "Invalid texture accepted.");
    require(!animation.load(texture, std::vector<AnimationFrame>{}, failure), "Empty sequence accepted.");
    require(animation.currentFrame() == 1 && animation.elapsedTime() == 0.125 && animation.isPlaying(),
        "Failed load altered playback.");
    for (const double value : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        bool rejected = false;
        try {
            animation.setSpeed(value);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected && animation.speed() == 1, "Invalid speed accepted or changed state.");
        rejected = false;
        try {
            animation.update(value);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected && animation.elapsedTime() == 0.125, "Invalid time accepted or changed state.");
    }
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

void padding(Renderer2D& renderer)
{
    Image image{5, 3, std::vector<std::uint8_t>(5 * 3 * 4, 255)};
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 5; ++x) {
            const auto offset = static_cast<std::size_t>((y * 5 + x) * 4);
            image.pixels[offset] = static_cast<std::uint8_t>(x * 51);
            image.pixels[offset + 1] = static_cast<std::uint8_t>(y * 100);
            image.pixels[offset + 2] = static_cast<std::uint8_t>((x + y) * 20);
        }
    }
    const auto texture = create(renderer, image, true);
    require(texture.width() == 5 && texture.height() == 3 && texture.image()->pixels == image.pixels,
        "Padding changed public dimensions or retained pixels.");
    RenderError error;
    frame(renderer);
    renderer.drawTexture(texture, Rect{0, 0, 137, 83});
    check(renderer.endFrame(error), error);
    GLint width = 0;
    GLint height = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    require(width == 8 && height == 4 && renderer.textureStats().gpuBytes == 128, "Unexpected padded GPU size.");
    std::array<std::uint8_t, 8 * 4 * 4> uploaded{};
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, uploaded.data());
    require(glGetError() == GL_NO_ERROR, "Cannot read padded texture.");
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 8; ++x) {
            for (int channel = 0; channel < 4; ++channel) {
                const auto target = static_cast<std::size_t>((y * 8 + x) * 4 + channel);
                const auto source = static_cast<std::size_t>(((y % 3) * 5 + x % 5) * 4 + channel);
                const int expected = x <= 5 ? image.pixels[source] : 0;
                require(uploaded[target] == expected, "GPU pixels were stretched or padding is incorrect.");
            }
        }
    }
    const auto sample = [&](int x, int y, TextureWrap wrapX, TextureWrap wrapY) {
        x = wrapX == TextureWrap::Repeat ? (x % 5 + 5) % 5 : std::clamp(x, 0, 4);
        y = wrapY == TextureWrap::Repeat ? (y % 3 + 3) % 3 : std::clamp(y, 0, 2);
        const auto offset = static_cast<std::size_t>((y * 5 + x) * 4);
        return Color{image.pixels[offset] / 255.0F, image.pixels[offset + 1] / 255.0F,
            image.pixels[offset + 2] / 255.0F};
    };
    for (const auto filter : {TextureFilter::Nearest, TextureFilter::Linear}) {
        for (const auto wrapX : {TextureWrap::Clamp, TextureWrap::Repeat}) {
            for (const auto wrapY : {TextureWrap::Clamp, TextureWrap::Repeat}) {
                TextureSampling options;
                options.filter = filter;
                options.wrapX = wrapX;
                options.wrapY = wrapY;
                options.repeats = {2.5F, 1.75F};
                frame(renderer);
                renderer.drawTexture(texture, Rect{0, 0, 137, 83}, DrawOptions{}, options);
                check(renderer.endFrame(error), error);
                for (int y = 0; y < 83; y += 7) {
                    for (int x = 0; x < 137; x += 9) {
                        const float u = (static_cast<float>(x) + 0.5F) / 137 * 5 * options.repeats.x;
                        const float v = (static_cast<float>(y) + 0.5F) / 83 * 3 * options.repeats.y;
                        Color expected;
                        if (filter == TextureFilter::Nearest) {
                            expected = sample(static_cast<int>(std::floor(u)), static_cast<int>(std::floor(v)), wrapX, wrapY);
                        } else {
                            const int left = static_cast<int>(std::floor(u - 0.5F));
                            const int top = static_cast<int>(std::floor(v - 0.5F));
                            const float dx = u - 0.5F - static_cast<float>(left);
                            const float dy = v - 0.5F - static_cast<float>(top);
                            const auto mix = [](Color a, Color b, float t) {
                                return Color{a.red + (b.red - a.red) * t, a.green + (b.green - a.green) * t,
                                    a.blue + (b.blue - a.blue) * t};
                            };
                            expected = mix(mix(sample(left, top, wrapX, wrapY), sample(left + 1, top, wrapX, wrapY), dx),
                                mix(sample(left, top + 1, wrapX, wrapY), sample(left + 1, top + 1, wrapX, wrapY), dx), dy);
                        }
                        pixel(x, y, expected, 3);
                    }
                }
            }
        }
    }
    Sprite region(texture, Rect{1, 1, 3, 1});
    region.size = {120, 40};
    frame(renderer);
    renderer.drawSprite(region);
    check(renderer.endFrame(error), error);
    pixel(0, 0, sample(1, 1, TextureWrap::Clamp, TextureWrap::Clamp));
    pixel(119, 39, sample(3, 1, TextureWrap::Clamp, TextureWrap::Clamp));
    // Nearest: поворот и отражение целочисленной области не захватывают соседей атласа.
    region.filter = TextureFilter::Nearest;
    region.transform = {{150, 20}, Pi / 2, {1, 1}, {}};
    frame(renderer);
    renderer.drawSprite(region);
    check(renderer.endFrame(error), error);
    for (int offset : {0, 1, 39, 40, 79, 80, 118, 119}) {
        pixel(130, 20 + offset, sample(1 + offset / 40, 1, TextureWrap::Clamp, TextureWrap::Clamp));
    }
    pixel(109, 80, Black);
    pixel(150, 80, Black);
    region.transform = {{150, 20}, 0, {-1, 2}, {}};
    frame(renderer);
    renderer.drawSprite(region);
    check(renderer.endFrame(error), error);
    pixel(30, 20, sample(3, 1, TextureWrap::Clamp, TextureWrap::Clamp));
    pixel(149, 99, sample(1, 1, TextureWrap::Clamp, TextureWrap::Clamp));
    pixel(29, 60, Black);
    pixel(150, 60, Black);
    TextureSampling excessive;
    excessive.wrapX = TextureWrap::Repeat;
    excessive.repeats.x = 1000000;
    frame(renderer);
    renderer.drawTexture(texture, Rect{0, 0, 137, 83}, DrawOptions{}, excessive);
    require(!renderer.endFrame(error) && error == RenderError::InvalidOptions, "Unbounded repeated geometry accepted.");
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
            require(mode == "pixels" || mode == "sampling" || mode == "integration" || mode == "sprites" || mode == "animation" || mode == "padding", "Unknown mode.");
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
                if (mode == "padding") { padding(renderer); }
                else if (mode == "animation") { animationChecks(renderer); }
                else if (mode == "pixels") { pixels(renderer, files); }
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
