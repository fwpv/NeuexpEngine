#include "application.h"
#include "entry_point.h"
#include "atlas.h"
#include "input.h"
#include <cmath>
#include <vector>

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
        for (int index = 0; index < 48; ++index) {
            Sprite sprite;
            if (!atlas.createSprite(index % 2 == 0 ? "ring.first" : "ring.second", sprite)) {
                error = {"Sprite demo", "Missing atlas region", {}};
                return false;
            }
            sprite.size = {56, 56};
            sprite.transform.origin = {28, 28};
            sprite.transform.position = {70 + static_cast<float>(index % 8) * 100, 120 + static_cast<float>(index / 8) * 90};
            sprites.push_back(sprite);
        }
        return true;
    }

    bool onRender(const FrameInfo& frame, Platform::Error& error) override
    {
        if (Platform::getInput().key(Key::Escape).pressed) {
            requestExit();
        }
        UiView view;
        UiError failure;
        if (!UiCanvas{}.createView(Viewport{0, 0, frame.window.width, frame.window.height}, view, failure)) {
            error = {"Sprite demo", "Invalid viewport", {}};
            return false;
        }
        beginFrame(frame.window.width, frame.window.height, Color{0.04F, 0.06F, 0.1F});
        getRenderer().setUiView(view);
        const auto& mouse = Platform::getInput().mouse();
        const Vector2 point{static_cast<float>(mouse.x), static_cast<float>(mouse.y)};
        for (auto& sprite : sprites) {
            sprite.transform.rotation = static_cast<float>(frame.elapsedTime) * 0.4F;
            const float scale = 1 + 0.2F * std::sin(static_cast<float>(frame.elapsedTime) + sprite.transform.position.x);
            sprite.transform.scale = {scale, scale};
            sprite.color = Color{1, 1, 1};
            if (mouse.inside && sprite.hitTest(point)) {
                sprite.color = sprite.hitTest(point, true) ? Color{0.4F, 1, 0.4F} : Color{1, 0.3F, 0.3F};
            }
            getRenderer().drawSprite(sprite);
        }
        getRenderer().drawDebugText("48 SPRITES / ONE ATLAS\nHOVER: GREEN = ALPHA HIT, RED = TRANSPARENT\nESC = EXIT", Vector2{20, 20}, Color{1, 1, 1});
        return endFrame(error);
    }

    std::vector<Sprite> sprites;
};
}
int Engine::applicationMain()
{
    SpriteApplication app;
    ApplicationOptions options;
    options.window.width = 860;
    options.window.height = 680;
    options.window.title = "Sprites and atlas";
    Platform::Error error;
    if (!app.run(options, error)) {
        Platform::reportError(error);
        return 1;
    }
    return 0;
}
