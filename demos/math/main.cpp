#include "entry_point.h"
#include "application.h"
#include "camera.h"
#include "geometry.h"
#include "input.h"
#include "math2d.h"
#include "platform.h"
#include "transform.h"
#include "renderer.h"
#include "ui.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace
{
class MathApplication final : public Engine::Application
{
private:
    bool onStart(Platform::Error& error) override
    {
        return initializeRenderer(error);
    }

    bool onInput(const Engine::FrameInfo&, Platform::Error&) override
    {
        if (Platform::isKeyPressed(Engine::Key::Escape)) {
            requestExit();
        }
        if (Platform::isKeyPressed(Engine::Key::R)) {
            object = Engine::Transform2{Engine::Vector2{400, 300}, 0, Engine::Vector2{1, 1}, Engine::Vector2{80, 50}};
        }
        if (Platform::isKeyPressed(Engine::Key::X)) {
            object.scale.x = -object.scale.x;
        }
        const float zoom = static_cast<float>(Platform::getInput().mouse().wheelY) * 0.1F;
        const float magnitude = std::clamp(std::abs(object.scale.x) + zoom, 0.25F, 3.0F);
        object.scale = Engine::Vector2{std::copysign(magnitude, object.scale.x), magnitude};
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
        constexpr float MovementSpeed = 200.0F; // Мировые единицы в секунду.
        constexpr float RotationSpeed = Engine::Pi / 2; // Радианы в секунду.
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
            error = Platform::Error{"Affine2::inverse",
                "Could not invert the object transform. TransformError: " + std::to_string(static_cast<int>(transformError)), std::nullopt};
            return false;
        }
        const auto& mouse = Platform::getInput().mouse();
        const Engine::Vector2 cursor = {static_cast<float>(mouse.x), static_cast<float>(mouse.y)};
        // В демо масштаб равномерный по модулю: обратное преобразование сохраняет форму круга.
        constexpr float CursorRadius = 12;
        const Engine::Circle localCursorCircle = {
            inverse.transformPoint(cursor), CursorRadius / std::abs(object.scale.x)
        };
        constexpr Engine::Rect LocalBounds = {0, 0, 160, 100};
        const bool inside = mouse.inside && Engine::contains(LocalBounds, localCursorCircle.center);
        const Engine::Rect bounds = forward.transformBounds(LocalBounds);

        Engine::UiView view;
        Engine::UiError uiError;
        if (!Engine::UiCanvas{}.createView(Engine::Viewport{0, 0, frame.window.width, frame.window.height}, view, uiError)) {
            error = Platform::Error{"UiCanvas::createView",
                "Could not calculate the math view. UiError: " + std::to_string(static_cast<int>(uiError)), std::nullopt};
            return false;
        }
        beginFrame(frame.window.width, frame.window.height, Engine::Color{0.03F, 0.04F, 0.07F});
        drawing.setUiView(view);
        Engine::DrawOptions shape;
        shape.transform = forward;
        shape.color = inside ? Engine::Color{0.2F, 0.85F, 0.35F} : Engine::Color{0.15F, 0.45F, 0.8F};
        drawing.drawRectangle(LocalBounds, shape);
        shape.color = Engine::Color{1, 1, 1};
        drawing.drawRectangle(Engine::Rect{28, 44, 80, 12}, shape);
        drawing.drawPolygon(std::vector<Engine::Vector2>{Engine::Vector2{132, 50},
            Engine::Vector2{108, 34}, Engine::Vector2{108, 66}}, shape);
        drawing.drawRectangle(bounds, Engine::Color{1, 0.6F, 0.2F}, Engine::ShapeFill::Outline);

        if (mouse.inside) {
            Engine::Color cursorColor = {1, 1, 1};
            if (Engine::intersects(bounds, Engine::Circle{cursor, CursorRadius})) {
                cursorColor = Engine::Color{1, 1, 0};
            }
            if (Engine::intersects(LocalBounds, localCursorCircle)) {
                cursorColor = Engine::Color{1, 0, 0};
            }
            drawing.drawCircle(Engine::Circle{cursor, CursorRadius}, cursorColor, Engine::ShapeFill::Outline, 32);
        }
        return endFrame(error);
    }

    Engine::Renderer2D& drawing = getRenderer();
    Engine::Transform2 object = {Engine::Vector2{400, 300}, 0, Engine::Vector2{1, 1}, Engine::Vector2{80, 50}};
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
