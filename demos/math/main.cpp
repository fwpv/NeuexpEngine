#include "app/entry_point.h"
#include "app/application.h"
#include "core/camera.h"
#include "core/geometry.h"
#include "app/input.h"
#include "core/math2d.h"
#include "app/platform.h"
#include "core/transform.h"
#include "graphics/renderer.h"
#include "graphics/ui_view.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

namespace
{
class MathApplication final : public NeuexpEngine::Application
{
private:
    bool onStart(Platform::Error& error) override
    {
        return initializeRenderer(error);
    }

    bool onInput(const NeuexpEngine::FrameInfo&, Platform::Error&) override
    {
        if (Platform::isKeyPressed(NeuexpEngine::Key::Escape)) {
            requestExit();
        }
        if (Platform::isKeyPressed(NeuexpEngine::Key::R)) {
            object = NeuexpEngine::Transform2{NeuexpEngine::Vector2{400, 300}, 0, NeuexpEngine::Vector2{1, 1}, NeuexpEngine::Vector2{80, 50}};
        }
        if (Platform::isKeyPressed(NeuexpEngine::Key::X)) {
            object.scale.x = -object.scale.x;
        }
        const float zoom = static_cast<float>(Platform::getInput().mouse().wheelY) * 0.1F;
        const float magnitude = std::clamp(std::abs(object.scale.x) + zoom, 0.25F, 3.0F);
        object.scale = NeuexpEngine::Vector2{std::copysign(magnitude, object.scale.x), magnitude};
        return true;
    }

    bool onUpdate(const NeuexpEngine::FrameInfo& frame, Platform::Error&) override
    {
        NeuexpEngine::Vector2 direction;
        if (Platform::isKeyDown(NeuexpEngine::Key::ArrowLeft)) {
            direction.x -= 1;
        }
        if (Platform::isKeyDown(NeuexpEngine::Key::ArrowRight)) {
            direction.x += 1;
        }
        if (Platform::isKeyDown(NeuexpEngine::Key::ArrowUp)) {
            direction.y -= 1;
        }
        if (Platform::isKeyDown(NeuexpEngine::Key::ArrowDown)) {
            direction.y += 1;
        }
        constexpr float MovementSpeed = 200.0F; // Мировые единицы в секунду.
        constexpr float RotationSpeed = NeuexpEngine::Pi / 2; // Радианы в секунду.
        object.position += NeuexpEngine::normalized(direction) * MovementSpeed * frame.deltaTime;
        if (Platform::isKeyDown(NeuexpEngine::Key::Q)) {
            object.rotation -= RotationSpeed * frame.deltaTime;
        }
        if (Platform::isKeyDown(NeuexpEngine::Key::E)) {
            object.rotation += RotationSpeed * frame.deltaTime;
        }
        return true;
    }

    bool onRender(const NeuexpEngine::FrameInfo& frame, Platform::Error& error) override
    {
        const NeuexpEngine::Affine2 forward = object.toAffine();
        NeuexpEngine::Affine2 inverse;
        if (!forward.inverse(inverse)) {
            error = Platform::Error{"Affine2::inverse",
                "Could not invert the object transform.", std::nullopt};
            return false;
        }
        const auto& mouse = Platform::getInput().mouse();
        const NeuexpEngine::Vector2 cursor = {static_cast<float>(mouse.x), static_cast<float>(mouse.y)};
        // В демо масштаб равномерный по модулю: обратное преобразование сохраняет форму круга.
        constexpr float CursorRadius = 12;
        const NeuexpEngine::Circle localCursorCircle = {
            inverse.transformPoint(cursor), CursorRadius / std::abs(object.scale.x)
        };
        constexpr NeuexpEngine::Rect LocalBounds = {0, 0, 160, 100};
        const bool inside = mouse.inside && NeuexpEngine::contains(LocalBounds, localCursorCircle.center);
        const NeuexpEngine::Rect bounds = forward.transformBounds(LocalBounds);

        NeuexpEngine::UiView view;
        NeuexpEngine::UiError uiError;
        if (!NeuexpEngine::UiCanvas{}.createView(NeuexpEngine::Viewport{0, 0, frame.window.width, frame.window.height}, view, uiError)) {
            error = Platform::Error{"UiCanvas::createView",
                "Could not calculate the math view. UiError: " + std::to_string(static_cast<int>(uiError)), std::nullopt};
            return false;
        }
        beginFrame(frame.window.width, frame.window.height, NeuexpEngine::Color{0.03F, 0.04F, 0.07F});
        drawing.setUiView(view);
        NeuexpEngine::DrawOptions shape;
        shape.transform = forward;
        shape.color = inside ? NeuexpEngine::Color{0.2F, 0.85F, 0.35F} : NeuexpEngine::Color{0.15F, 0.45F, 0.8F};
        drawing.drawRectangle(LocalBounds, shape);
        shape.color = NeuexpEngine::Color{1, 1, 1};
        drawing.drawRectangle(NeuexpEngine::Rect{28, 44, 80, 12}, shape);
        drawing.drawPolygon(NeuexpEngine::Polygon{NeuexpEngine::Vector2{132, 50},
            NeuexpEngine::Vector2{108, 34}, NeuexpEngine::Vector2{108, 66}}, shape);
        drawing.drawRectangle(bounds, NeuexpEngine::Color{1, 0.6F, 0.2F}, NeuexpEngine::ShapeFill::Outline);

        if (mouse.inside) {
            NeuexpEngine::Color cursorColor = {1, 1, 1};
            if (NeuexpEngine::intersects(bounds, NeuexpEngine::Circle{cursor, CursorRadius})) {
                cursorColor = NeuexpEngine::Color{1, 1, 0};
            }
            if (NeuexpEngine::intersects(LocalBounds, localCursorCircle)) {
                cursorColor = NeuexpEngine::Color{1, 0, 0};
            }
            drawing.drawCircle(NeuexpEngine::Circle{cursor, CursorRadius}, cursorColor, NeuexpEngine::ShapeFill::Outline, 32);
        }
        return endFrame(error);
    }

    NeuexpEngine::Renderer2D& drawing = getRenderer();
    NeuexpEngine::Transform2 object = {NeuexpEngine::Vector2{400, 300}, 0, NeuexpEngine::Vector2{1, 1}, NeuexpEngine::Vector2{80, 50}};
};
}

int NeuexpEngine::applicationMain()
{
    NeuexpEngine::ApplicationOptions options;
    options.window.title = "Math | arrows: move | Q/E: rotate | wheel: scale | X: reflect | R: reset | hover: local hit test";
    MathApplication application;
    Platform::Error failure;
    const bool success = application.run(options, failure);
    if (!success) {
        Platform::reportError(failure);
    }
    return success ? 0 : 1;
}
