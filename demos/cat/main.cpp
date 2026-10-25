#include "game.h"
#include "application.h"
#include "entry_point.h"
#include "resources.h"
#include "transform.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

namespace
{
using namespace Engine;
constexpr Color Background = { 245.0F / 255, 250.0F / 255, 245.0F / 255 };
constexpr std::array<const char*, static_cast<std::size_t>(Cat::Art::Count)> ArtFiles = {{
    "platform.png", "platform2.png", "platform3.png", "grib.png", "flower2.png", "flower1.png",
    "head1.png", "head2.png", "head3.png", "head4.png", "head5.png", "feet.png",
    "label1.png", "label2.png", "label3.png"
}};
constexpr std::array<Cat::Art, 5> HeadFrames = {{ Cat::Art::Head5, Cat::Art::Head4,
    Cat::Art::Head1, Cat::Art::Head2, Cat::Art::Head3 }};

class CatApplication final : public Application
{
    Renderer2D& renderer = getRenderer();
    ResourceFiles files;
    Cat::Game game;
    std::array<Texture, static_cast<std::size_t>(Cat::Art::Count)> textures;
    std::array<Texture, 12> feet;
    RenderError renderError = RenderError::None;
    bool verticalSync = false;
    bool syncUnavailable = false;

    const Texture& texture(Cat::Art art) const
    {
        return textures[static_cast<std::size_t>(art)];
    }

    bool failure(const std::string& message, Platform::Error& error)
    {
        error = { "cat", message, {} };
        return false;
    }

    bool crop(const Image& source, int x, int y, int width, int height,
        Texture& result, ResourceError& error)
    {
        Image image;
        image.width = width;
        image.height = height;
        image.pixels.reserve(static_cast<std::size_t>(width * height * 4));
        for (int row = y; row < y + height; ++row) {
            const auto begin = source.pixels.begin() + (row * source.width + x) * 4;
            image.pixels.insert(image.pixels.end(), begin, begin + width * 4);
        }
        return renderer.createTexture(image, result, error);
    }

    bool loadArt(Platform::Error& error)
    {
        ResourceError resourceError;
        for (std::size_t index = 0; index < textures.size(); ++index) {
            Image image;
            if (!files.loadPng(std::string("cat/") + ArtFiles[index], image, resourceError)) {
                return failure(resourceError.message, error);
            }
            // SDL_DisplayFormat + чёрный ColorKey в оригинале игнорируют альфа-канал PNG.
            for (std::size_t pixel = 0; pixel < image.pixels.size(); pixel += 4) {
                image.pixels[pixel + 3] = image.pixels[pixel] == 0 && image.pixels[pixel + 1] == 0
                    && image.pixels[pixel + 2] == 0 ? 0 : 255;
            }
            if (index == static_cast<std::size_t>(Cat::Art::Feet)) {
                for (int frame = 0; frame < static_cast<int>(feet.size()); ++frame) {
                    if (!crop(image, frame % 2 * 180, frame / 2 * 53, 180, 53,
                            feet[static_cast<std::size_t>(frame)], resourceError)) {
                        return failure(resourceError.message, error);
                    }
                }
            } else {
                if (index >= static_cast<std::size_t>(Cat::Art::Leaf1)
                    && index <= static_cast<std::size_t>(Cat::Art::Leaf3)) {
                    // Поле до степени двойки сохраняет исходные тексели при вращении в OpenGL 1.1.
                    Image padded{ 64, 64, std::vector<std::uint8_t>(64 * 64 * 4) };
                    for (int row = 0; row < image.height; ++row) {
                        std::copy_n(image.pixels.begin() + row * image.width * 4, image.width * 4,
                            padded.pixels.begin() + row * padded.width * 4);
                    }
                    image = std::move(padded);
                }
                if (!renderer.createTexture(image, textures[index], resourceError)) {
                    return failure(resourceError.message, error);
                }
            }
        }
        return true;
    }

    bool onStart(Platform::Error& error) override
    {
        ResourceError resourceError;
        if (!files.initialize(resourceError)) { return failure(resourceError.message, error); }
        if (!renderer.initialize(renderError)) { return failure("Cannot initialize renderer.", error); }
        Platform::Error syncError;
        syncUnavailable = !setVSync(false, syncError);
        return loadArt(error);
    }

    bool onInput(const FrameInfo&, Platform::Error&) override
    {
        if (Platform::isKeyPressed(Key::Escape)) { requestExit(); }
        if (Platform::isKeyPressed(Key::V)) {
            const bool enabled = !verticalSync;
            Platform::Error syncError;
            syncUnavailable = !setVSync(enabled, syncError);
            if (!syncUnavailable) { verticalSync = enabled; }
        }
        if (Platform::isKeyPressed(Key::R)) {
            game = Cat::Game{};
        }
        return true;
    }

    bool onUpdate(const FrameInfo& frame, Platform::Error&) override
    {
        const Cat::Controls controls = { Platform::isKeyDown(Key::ArrowRight), Platform::isKeyDown(Key::ArrowLeft),
            Platform::isKeyDown(Key::ArrowUp), Platform::isKeyDown(Key::ArrowDown) };
        game.update(frame.deltaTime, controls);
        return true;
    }

    bool picture(const Texture& image, double x, double y, double scale = 1)
    {
        TextureSampling sampling;
        sampling.filter = TextureFilter::Nearest;
        return renderer.drawTexture(image, { static_cast<float>(x), static_cast<float>(y),
            static_cast<float>(image.width() * scale), static_cast<float>(image.height() * scale) }, {}, renderError, sampling);
    }

    bool onPrepareFrame(const FrameInfo& frame, Platform::Error& error) override
    {
        const auto fps = static_cast<int>(std::round(frame.framesPerSecond));
        return Platform::setWindowTitle("CAT | " + std::to_string(fps) + " FPS | "
            + (verticalSync ? "VSync ON" : "Limit 100 FPS")
            + (syncUnavailable ? " / VSync control unavailable" : (verticalSync ? "" : " / VSync OFF"))
            + " | V: VSync / R: restart", error);
    }

    bool drawLeaves()
    {
        TextureSampling sampling;
        sampling.filter = TextureFilter::Nearest;
        for (const auto& leaf : game.leaves) {
            if (!leaf.active || leaf.x < game.cameraX - 25 || leaf.x > game.cameraX + 960
                || leaf.y < -25 || leaf.y > 725) { continue; }
            const auto& image = texture(leaf.art);
            const float width = static_cast<float>(image.width());
            const float height = static_cast<float>(image.height());
            const int x = static_cast<int>(leaf.x - game.cameraX);
            const int y = static_cast<int>(leaf.y);
            DrawOptions options;
            Transform2 transform;
            transform.position = { static_cast<float>(x) + 17.5F, static_cast<float>(y) + 17.5F };
            transform.rotation = -degreesToRadians(static_cast<float>(leaf.angle));
            options.transform = transform.toAffine();
            if (!renderer.drawTexture(image, { -17.5F, -17.5F, width, height },
                    options, renderError, sampling)) { return false; }
        }
        return true;
    }

    bool drawPlatforms()
    {
        for (const auto& slot : game.platforms) {
            if (!slot) { continue; }
            const auto& platform = *slot;
            const double screenX = platform.x - game.cameraX;
            if (platform.art != Cat::Art::MovingPlatform && (screenX <= -250 || screenX >= 940)) { continue; }
            const int x = static_cast<int>(screenX);
            const int y = static_cast<int>(platform.y);
            if (!picture(texture(platform.art), x, y)) { return false; }
            for (const auto& decoration : platform.decorations) {
                if (!picture(texture(decoration.art), x + decoration.x, y + decoration.y)) { return false; }
            }
        }
        return true;
    }

    bool drawCat()
    {
        constexpr double PlatformInset = 3;
        constexpr double FeetSpriteX = 44 * Cat::CatScale;
        constexpr double FeetSpriteY = 120 * Cat::CatScale;
        const auto art = HeadFrames[static_cast<std::size_t>(game.player.head - 1)];
        const double y = game.player.y + PlatformInset;
        return picture(texture(art), Cat::CatScreenX, y, Cat::CatScale)
            && picture(feet[static_cast<std::size_t>(game.player.feet - 1)], Cat::CatScreenX + FeetSpriteX,
                y + FeetSpriteY, Cat::CatScale);
    }

    bool onRender(const FrameInfo& frame, Platform::Error& error) override
    {
        if (frame.window.width <= 0 || frame.window.height <= 0) { return true; }
        const float scale = std::min(static_cast<float>(frame.window.width) / Cat::CanvasWidth,
            static_cast<float>(frame.window.height) / Cat::CanvasHeight);
        const int width = static_cast<int>(Cat::CanvasWidth * scale);
        const int height = static_cast<int>(Cat::CanvasHeight * scale);
        UiView ui;
        UiError uiError;
        if (!UiCanvas{ scale }.createView({ (frame.window.width - width) / 2,
                (frame.window.height - height) / 2, width, height }, ui, uiError)) {
            return failure("Cannot calculate game viewport.", error);
        }
        if (!renderer.beginFrame(frame.window.width, frame.window.height, Background, renderError)
            || !renderer.beginUi(ui, renderError) || !drawLeaves()
            || !drawPlatforms() || !drawCat() || !renderer.endFrame(renderError)) {
            return failure("Rendering failed (RenderError=" + std::to_string(static_cast<int>(renderError)) + ").", error);
        }
        return true;
    }
};
}

int Engine::applicationMain()
{
    CatApplication application;
    ApplicationOptions options;
    options.window.width = Cat::CanvasWidth;
    options.window.height = Cat::CanvasHeight;
    options.window.minimumWidth = Cat::CanvasWidth;
    options.window.minimumHeight = Cat::CanvasHeight;
    options.window.title = "CAT";
    options.maxDeltaTime = 0.2F;
    options.targetFramesPerSecond = 100;
    std::vector<Platform::Error> errors;
    if (!application.run(options, errors)) {
        for (const auto& error : errors) { Platform::reportError(error); }
        return 1;
    }
    return 0;
}
