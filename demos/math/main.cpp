#include "entry_point.h"
#include "application.h"
#include "transform.h"
#include "../common/painter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr Engine::Rect LocalBounds = { 0, 0, 160, 100 };
constexpr Engine::Color IdleColor = { 0.15F, 0.45F, 0.8F };
constexpr Engine::Color HitColor = { 0.2F, 0.85F, 0.35F };
constexpr Engine::Color BoundsColor = { 1, 0.6F, 0.2F };
constexpr Engine::Color CursorColor = { 1, 1, 1 };
constexpr Engine::Color CursorBoundsColor = { 1, 1, 0 };
constexpr Engine::Color CursorHitColor = { 1, 0, 0 };
constexpr Engine::Color DirectionColor = { 1, 1, 1 };
constexpr Engine::Rect DirectionShaft = { 28, 44, 80, 12 };
constexpr std::array<Engine::Vector2, 3> DirectionHead = {{ { 132, 50 }, { 108, 34 }, { 108, 66 } }};
constexpr float MovementSpeed = 200;
constexpr float RotationSpeed = Engine::Pi / 2;
constexpr float CursorRadius = 12;
constexpr int CircleSegments = 32;

class MathApplication final : public Engine::Application
{
private:
    Demo::Painter drawing{ getRenderer() };

    bool onStart(Platform::Error& error) override
    {
        return drawing.initialize(error);
    }

    Engine::Transform2 object = { { 400, 300 }, 0, { 1, 1 }, { 80, 50 } };

    bool onInput(const Engine::FrameInfo&, Platform::Error&) override
    {
        if (Platform::isKeyPressed(Engine::Key::Escape)) {
            requestExit();
        }
        if (Platform::isKeyPressed(Engine::Key::R)) {
            object = { { 400, 300 }, 0, { 1, 1 }, { 80, 50 } };
        }
        if (Platform::isKeyPressed(Engine::Key::X)) {
            object.scale.x = -object.scale.x;
        }
        const float zoom = static_cast<float>(Platform::getInput().mouse().wheelY) * 0.1F;
        const float magnitude = std::clamp(std::abs(object.scale.x) + zoom, 0.25F, 3.0F);
        object.scale = { std::copysign(magnitude, object.scale.x), magnitude };
        return true;
    }

    bool onUpdate(const Engine::FrameInfo& frame, Platform::Error&) override
    {
        Engine::Vector2 direction;
        if (Platform::isKeyDown(Engine::Key::ArrowLeft)) {
            direction.x -= 1;
        }
        if (Platform::isKeyDown(Engine::Key::ArrowRight)) {
            direction.x += 1;
        }
        if (Platform::isKeyDown(Engine::Key::ArrowUp)) {
            direction.y -= 1;
        }
        if (Platform::isKeyDown(Engine::Key::ArrowDown)) {
            direction.y += 1;
        }
        object.position += Engine::normalized(direction) * MovementSpeed * frame.deltaTime;
        if (Platform::isKeyDown(Engine::Key::Q)) {
            object.rotation -= RotationSpeed * frame.deltaTime;
        }
        if (Platform::isKeyDown(Engine::Key::E)) {
            object.rotation += RotationSpeed * frame.deltaTime;
        }
        return true;
    }

    bool onRender(const Engine::FrameInfo& frame, Platform::Error& error) override
    {
        const Engine::Affine2 forward = object.toAffine();
        Engine::Affine2 inverse;
        Engine::TransformError transformError;
        if (!forward.inverse(inverse, transformError)) {
            error = { "Affine2::inverse", "Object transform is not invertible.", {} };
            return false;
        }
        const auto& mouse = Platform::getInput().mouse();
        const Engine::Vector2 cursor = { static_cast<float>(mouse.x), static_cast<float>(mouse.y) };
        // В демо масштаб равномерный по модулю: обратное преобразование сохраняет форму круга.
        const Engine::Circle localCursorCircle = {
            inverse.transformPoint(cursor), CursorRadius / std::abs(object.scale.x)
        };
        const bool inside = mouse.inside && Engine::contains(LocalBounds, localCursorCircle.center);
        const Engine::Rect bounds = forward.transformBounds(LocalBounds);

        Engine::UiView view;
        Engine::UiError uiError;
        if (!Engine::UiCanvas{}.createView({ 0, 0, frame.window.width, frame.window.height }, view, uiError)) {
            error = { "UiCanvas::createView", "Could not calculate the math view.", {} };
            return false;
        }
        drawing.beginFrame(frame, { 0.03F, 0.04F, 0.07F });
        drawing.beginUi(view);
        drawing.options.transform = forward;
        drawing.rectangle(LocalBounds, inside ? HitColor : IdleColor);
        drawing.rectangle(DirectionShaft, DirectionColor);
        drawing.polygon({ DirectionHead.begin(), DirectionHead.end() });
        drawing.options.transform = {};
        drawing.rectangle(bounds, BoundsColor, false);

        if (mouse.inside) {
            Engine::Color cursorColor = CursorColor;
            if (Engine::intersects(bounds, Engine::Circle{ cursor, CursorRadius })) {
                cursorColor = CursorBoundsColor;
            }
            if (Engine::intersects(LocalBounds, localCursorCircle)) {
                cursorColor = CursorHitColor;
            }
            drawing.circle({ cursor, CursorRadius }, cursorColor, false, CircleSegments);
        }
        return drawing.endFrame(error);
    }
};
}

int Engine::applicationMain()
{
    Engine::ApplicationOptions options;
    options.window.title = "Math | arrows: move | Q/E: rotate | wheel: scale | X: reflect | R: reset | hover: local hit test";
    MathApplication application;
    std::vector<Platform::Error> errors;
    const bool success = application.run(options, errors);
    bool showDialog = true;
    for (const auto& error : errors) {
        Platform::reportError(error, showDialog);
        showDialog = false;
    }
    return success ? 0 : 1;
}
