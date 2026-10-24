#include "application.h"
#include "entry_point.h"
#include "resources.h"
#include "../common/renderer_text.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>

namespace
{
using namespace Engine;
constexpr Color Background = { 0.055F, 0.085F, 0.13F };
constexpr Color Panel = { 0.10F, 0.16F, 0.22F };
constexpr Color White = { 0.92F, 0.95F, 1 };
constexpr Color Mint = { 0.40F, 0.93F, 0.70F };
constexpr Color Gold = { 1, 0.77F, 0.35F };
constexpr float CanvasWidth = 1100;
constexpr float CanvasHeight = 790;

class TextureApplication final : public Application
{
    Renderer2D& renderer = getRenderer();
    ResourceFiles files;
    std::array<Texture, 3> heads;
    Texture apple;
    Texture sharedApple;
    Texture platform;
    Texture checker;
    UiView ui;
    std::string caption;
    std::string status = "READY / E TESTS A BROKEN PNG";
    TextureFilter filter = TextureFilter::Linear;
    TextureWrap wrap = TextureWrap::Repeat;
    RenderError renderError = RenderError::None;
    int reloads = 0;

    bool load(Platform::Error& error)
    {
        ResourceError resourceError;
        std::vector<std::uint8_t> raw;
        // Несжатый ресурс сначала проверяет путь CPU -> GPU независимо от декодера PNG.
        if (!files.readBinary("textures/checker.rgba", raw, resourceError)
            || !renderer.createTexture({ 4, 4, std::move(raw) }, checker, resourceError)) {
            error = { "Texture resources", resourceError.message, {} };
            return false;
        }
        for (std::size_t index = 0; index < heads.size(); ++index) {
            const auto name = "textures/head" + std::to_string(index + 1) + ".png";
            if (!renderer.loadTexture(files, name, heads[index], resourceError, true)) {
                error = { "Texture resources", resourceError.message, {} };
                return false;
            }
        }
        if (!renderer.loadTexture(files, "textures/apple.png", apple, resourceError)
            || !renderer.loadTexture(files, "textures/./apple.png", sharedApple, resourceError)
            || !renderer.loadTexture(files, "textures/platform.png", platform, resourceError)) {
            error = { "Texture resources", resourceError.message, {} };
            return false;
        }
        return true;
    }

    bool onStart(Platform::Error& error) override
    {
        ResourceError resourceError;
        if (!files.initialize(resourceError) || !files.readText("textures/caption.txt", caption, resourceError)) {
            error = { "Texture resources", resourceError.message, {} };
            return false;
        }
        if (!renderer.initialize(renderError)) {
            error = { "Texture renderer", "Renderer initialization failed.", {} };
            return false;
        }
        return load(error);
    }

    bool onInput(const FrameInfo&, Platform::Error& error) override
    {
        if (Platform::isKeyPressed(Key::Escape)) { requestExit(); }
        if (Platform::isKeyPressed(Key::F)) {
            filter = filter == TextureFilter::Linear ? TextureFilter::Nearest : TextureFilter::Linear;
        }
        if (Platform::isKeyPressed(Key::W)) {
            wrap = wrap == TextureWrap::Repeat ? TextureWrap::Clamp : TextureWrap::Repeat;
        }
        if (Platform::isKeyPressed(Key::E)) {
            Texture broken;
            ResourceError resourceError;
            if (!renderer.loadTexture(files, "textures/broken.png", broken, resourceError)) {
                status = "BROKEN PNG REJECTED / GALLERY STILL RUNNING";
                std::cout << resourceError.message << '\n';
            } else {
                error = { "Texture gallery", "A broken PNG was unexpectedly accepted.", {} };
                return false;
            }
        }
        if (Platform::isKeyPressed(Key::R)) {
            ResourceError resourceError;
            for (int index = 0; index < 100; ++index) {
                if (!renderer.loadTexture(files, "textures/apple.png", sharedApple, resourceError)) {
                    error = { "Texture cache", resourceError.message, {} };
                    return false;
                }
            }
            reloads += 100;
            status = "100 CACHE REQUESTS / GPU COUNT STAYS THE SAME";
        }
        if (Platform::isKeyPressed(Key::Space)) {
            renderer.shutdown();
            if (!renderer.initialize(renderError) || !load(error)) {
                return false;
            }
            status = "TEXTURES RELEASED AND RECREATED";
        }
        return true;
    }

    bool rectangle(const Rect& bounds, Color color)
    {
        DrawOptions options;
        options.color = color;
        return renderer.drawRectangle(bounds, ShapeFill::Filled, options, renderError);
    }

    bool text(const std::string& value, Vector2 position, Color color = White, float size = 2)
    {
        return Demo::text(renderer, value, position, color, renderError, size);
    }

    bool picture(const Texture& texture, const Rect& bounds, float alpha = 1,
        const TextureSampling& sampling = {})
    {
        DrawOptions options;
        options.color.alpha = alpha;
        return renderer.drawTexture(texture, bounds, options, renderError, sampling);
    }

    bool gallery()
    {
        TextureSampling sampling;
        sampling.filter = filter;
        if (!text(caption, { 28, 22 }, Mint, 3)
            || !text("F FILTER / W WRAP / R CACHE / E BROKEN PNG / SPACE RELOAD / ESC EXIT", { 28, 57 }, White, 1.6F)
            || !rectangle({ 24, 95, 1052, 280 }, Panel)
            || !text("PNG / ORIGINAL CAT ART / TRANSPARENCY", { 44, 115 }, Mint)) {
            return false;
        }
        for (std::size_t index = 0; index < heads.size(); ++index) {
            const float x = 90 + static_cast<float>(index) * 340;
            if (!picture(heads[index], { x, 160, 270, 160 }, 1, sampling)
                || !picture(platform, { x + 30, 325, 210, 42 }, 1, sampling)) {
                return false;
            }
        }
        if (!rectangle({ 24, 392, 510, 215 }, Panel)
            || !text("RAW RGBA / SAMPLING", { 44, 412 }, Mint)
            || !picture(checker, { 44, 450, 132, 132 }, 1, sampling)
            || !text(filter == TextureFilter::Linear ? "LINEAR" : "NEAREST", { 44, 586 }, Gold, 1.5F)) {
            return false;
        }
        sampling.wrapX = wrap;
        sampling.wrapY = wrap;
        sampling.repeats = { 3, 3 };
        if (!picture(checker, { 206, 450, 300, 132 }, 1, sampling)
            || !text(wrap == TextureWrap::Repeat ? "REPEAT 3 X 3" : "CLAMP 3 X 3", { 210, 586 }, Gold, 1.5F)
            || !rectangle({ 552, 392, 524, 215 }, Panel)
            || !text("SHARED IMAGE / ALPHA BLENDING", { 572, 412 }, Mint)) {
            return false;
        }
        for (int index = 0; index < 6; ++index) {
            const float x = 580 + static_cast<float>(index) * 75;
            if (!picture(index % 2 ? sharedApple : apple, { x, 468, 70, 81 },
                    1 - static_cast<float>(index) * 0.13F)) {
                return false;
            }
        }
        const auto stats = renderer.textureStats();
        return text(apple == sharedApple ? "ALL APPLES SHARE ONE GPU TEXTURE" : "CACHE ERROR", { 572, 574 }, Gold, 1.5F)
            && text("GPU TEXTURES " + std::to_string(stats.count) + " / " + std::to_string(stats.gpuBytes / 1024)
                + " KIB / CACHE REQUESTS " + std::to_string(reloads), { 28, 632 }, White, 1.8F)
            && text("CPU PIXELS " + std::to_string(stats.retainedPixelBytes / 1024)
                + " KIB / ORIGINAL HEAD IMAGES RETAINED", { 28, 661 }, White, 1.8F)
            && text(status, { 28, 704 }, Gold, 1.8F)
            && text("ASSETS ARE LOADED NEXT TO THE EXECUTABLE / ANY WORKING DIRECTORY", { 28, 753 }, Mint, 1.5F);
    }

    bool onRender(const FrameInfo& frame, Platform::Error& error) override
    {
        if (frame.window.width <= 0 || frame.window.height <= 0) { return true; }
        const float scale = std::min(static_cast<float>(frame.window.width) / CanvasWidth,
            static_cast<float>(frame.window.height) / CanvasHeight);
        UiError uiError;
        if (!UiCanvas{ scale }.createView({ 0, 0, frame.window.width, frame.window.height }, ui, uiError)) {
            error = { "Texture gallery", "Cannot create UI view.", {} };
            return false;
        }
        if (!renderer.beginFrame(frame.window.width, frame.window.height, Background, renderError)
            || !renderer.beginUi(ui, renderError) || !gallery() || !renderer.endFrame(renderError)) {
            error = { "Texture gallery", "Rendering failed (RenderError="
                + std::to_string(static_cast<int>(renderError)) + ").", {} };
            return false;
        }
        return true;
    }
};
}

int Engine::applicationMain()
{
    TextureApplication application;
    ApplicationOptions options;
    options.window.width = static_cast<int>(CanvasWidth);
    options.window.height = static_cast<int>(CanvasHeight);
    options.window.minimumWidth = 700;
    options.window.minimumHeight = 500;
    options.window.title = "Cat texture gallery";
    std::vector<Platform::Error> errors;
    if (!application.run(options, errors)) {
        for (const auto& error : errors) { Platform::reportError(error); }
        return 1;
    }
    return 0;
}
