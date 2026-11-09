#include "application.h"
#include "entry_point.h"
#include "atlas.h"
#include "animation.h"
#include "input.h"
#include <cmath>
#include <vector>
#include <string>
#include <cstdlib>

namespace
{
using namespace Engine;
class SpriteApplication final : public Application
{
private:
    bool onStart(Platform::Error& error) override
    {
        if (!initializeRenderer(error)) {
            return false;
        }
        Image image{64, 32, std::vector<std::uint8_t>(64 * 32 * 4, 255)};
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 64; ++x) {
                const int dx = x % 32 - 16;
                const int dy = y - 16;
                const auto offset = static_cast<std::size_t>((y * 64 + x) * 4);
                image.pixels[offset] = x < 32 ? 60 : 240;
                image.pixels[offset + 1] = x < 32 ? 220 : 130;
                image.pixels[offset + 2] = 180;
                image.pixels[offset + 3] = dx * dx + dy * dy < 200 && dx * dx + dy * dy > 35 ? 255 : 0;
            }
        }
        Texture texture;
        ResourceError failure;
        ResourceFiles files;
        TextureAtlas atlas;
        if (!getRenderer().createTexture(image, texture, failure, true)
            || !files.initialize(failure) || !atlas.load(texture, files, "sprites.atlas", failure)) {
            error = {"Sprite demo", failure.message, {}};
            return false;
        }
        const Rect* first = atlas.find("ring.first");
        const Rect* second = atlas.find("ring.second");
        if (!first || !second) {
            error = {"Sprite demo", "Missing atlas region", {}};
            return false;
        }
        for (int index = 0; index < 48; ++index) {
            Animation sprite;
            if (!sprite.load(texture, std::vector<AnimationFrame>{
                    AnimationFrame{*first, 0.4}, AnimationFrame{*second, 0.6}}, failure)) {
                error = {"Sprite demo", failure.message, {}};
                return false;
            }
            sprite.setMode(index < 8 ? AnimationMode::Once : AnimationMode::Loop);
            sprite.setSpeed(0.5 + static_cast<double>(index % 8) * 0.25);
            sprite.play();
            sprite.size = {56, 56};
            sprite.transform.origin = {28, 28};
            sprite.transform.position = {70 + static_cast<float>(index % 8) * 100, 160 + static_cast<float>(index / 8) * 85};
            sprites.push_back(sprite);
        }
        return true;
    }

    bool onInput(const FrameInfo&, Platform::Error&) override
    {
        const auto& input = Platform::getInput();
        if (input.key(Key::Escape).pressed) {
            requestExit();
        }
        if (input.key(Key::Space).pressed) {
            setPaused(!isPaused());
        }
        for (auto& sprite : sprites) {
            if (input.key(Key::P).pressed) {
                sprite.pause();
            }
            if (input.key(Key::Enter).pressed) {
                sprite.play();
            }
            if (input.key(Key::S).pressed) {
                sprite.stop();
            }
            if (input.key(Key::R).pressed) {
                sprite.restart();
            }
        }
        return true;
    }

    bool onUpdate(const FrameInfo& frame, Platform::Error&) override
    {
        for (auto& sprite : sprites) {
            sprite.update(frame.deltaTime);
            sprite.transform.position.x = 30 + std::fmod(sprite.transform.position.x - 30 + 20 * frame.deltaTime, 800.0F);
            sprite.transform.rotation = static_cast<float>(frame.elapsedTime) * 0.4F;
            const float scale = 1 + 0.2F * std::sin(static_cast<float>(frame.elapsedTime) + sprite.transform.position.x);
            sprite.transform.scale = {scale, scale};
        }
        return true;
    }

    bool onPrepareFrame(const FrameInfo&, Platform::Error&) override
    {
        const auto& mouse = Platform::getInput().mouse();
        const Vector2 point{static_cast<float>(mouse.x), static_cast<float>(mouse.y)};
        for (auto& sprite : sprites) {
            sprite.color = Color{1, 1, 1};
            if (mouse.inside && sprite.hitTest(point)) {
                sprite.color = sprite.hitTest(point, true) ? Color{0.4F, 1, 0.4F} : Color{1, 0.3F, 0.3F};
            }
        }
        return true;
    }

    bool onRender(const FrameInfo& frame, Platform::Error& error) override
    {
        UiView view;
        UiError failure;
        if (!UiCanvas{}.createView(Viewport{0, 0, frame.window.width, frame.window.height}, view, failure)) {
            error = {"Sprite demo", "Invalid viewport", {}};
            return false;
        }
        beginFrame(frame.window.width, frame.window.height, Color{0.04F, 0.06F, 0.1F});
        getRenderer().setUiView(view);
        std::size_t completed = 0;
        for (const auto& sprite : sprites) {
            getRenderer().drawSprite(sprite);
            if (sprite.isFinished()) {
                ++completed;
            }
        }
        const std::string label = "48 ANIMATIONS / TOP ROW: ONCE / OTHERS: LOOP\n"
            "SPACE: GLOBAL PAUSE / P: PAUSE / ENTER: PLAY / S: STOP / R: RESTART\n"
            "HOVER: GREEN = ALPHA HIT, RED = TRANSPARENT / ESC: EXIT\n"
            "FINISHED: " + std::to_string(completed) + " / FPS: " + std::to_string(static_cast<int>(frame.framesPerSecond));
        getRenderer().drawDebugText(label, Vector2{20, 20}, Color{1, 1, 1}, 1);
        return endFrame(error);
    }

    std::vector<Animation> sprites;
};
}
int Engine::applicationMain()
{
    SpriteApplication app;
    ApplicationOptions options;
    options.window.width = 860;
    options.window.height = 680;
    options.window.title = "Sprites and animation";
    // Позволяет сравнивать движение при разных FPS без изменения игрового времени.
    if (const char* value = std::getenv("OGL_DEMO_FPS")) {
        char* end = nullptr;
        const double fps = std::strtod(value, &end);
        if (end == value || *end != '\0' || !std::isfinite(fps) || fps <= 0) {
            Platform::reportError(Platform::Error{"Sprite demo", "OGL_DEMO_FPS must be a positive number.", {}});
            return 1;
        }
        options.targetFramesPerSecond = fps;
    }
    Platform::Error error;
    if (!app.run(options, error)) {
        Platform::reportError(error);
        return 1;
    }
    return 0;
}
