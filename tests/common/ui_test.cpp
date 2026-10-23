#include "ui.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float actual, float expected)
{
    return std::isfinite(actual) && std::abs(actual - expected) < 0.001F;
}

bool near(Engine::Vector2 actual, Engine::Vector2 expected)
{
    return near(actual.x, expected.x) && near(actual.y, expected.y);
}
}

int main()
{
    try {
        using namespace Engine;
        UiCanvas canvas;
        UiView view;
        UiError error = UiError::InvalidScale;
        require(canvas.createView({ 0, 0, 800, 600 }, view, error) && error == UiError::None, "Default UI failed.");
        require(view.size == Size2{ 800, 600 } && near(view.uiToScreen({ 40, 30 }), { 40, 30 }), "Default UI units failed.");
        Vector2 point;
        for (float scale : { 0.75F, 1.25F, 1.5F, 2.0F, 2.5F }) {
            canvas.scale = scale;
            require(canvas.createView({ 40, 70, 801, 603 }, view, error), "Scaled UI failed.");
            require(near(view.size.width, 801 / scale) && near(view.size.height, 603 / scale), "Logical size failed.");
            require(near(view.uiToScreen({ 100, 50 }), { 40 + 100 * scale, 70 + 50 * scale }), "Offset UI mapping failed.");
            require(view.screenToUi(view.uiToScreen({ 100, 50 }), point) && near(point, { 100, 50 }), "UI mouse round trip failed.");
            require(view.screenToUi({ 40, 70 }, point) && near(point, {}), "UI origin rejected.");
            require(view.screenToUi({ 840, 672 }, point), "Last client pixel rejected.");
            point = { 17, 19 };
            require(!view.screenToUi({ 841, 200 }, point) && !view.screenToUi({ 300, 673 }, point)
                && !view.screenToUi({ 39, 70 }, point) && !view.screenToUi({ 40, 69 }, point)
                && point == Vector2{ 17, 19 }, "UI boundary failed or changed output.");
            require(!view.screenToUi({ std::numeric_limits<float>::quiet_NaN(), 70 }, point)
                && !view.screenToUi({ 40, std::numeric_limits<float>::infinity() }, point), "Invalid mouse accepted.");
        }

        Camera2 camera;
        CameraView world;
        CameraError cameraError;
        camera.position = { 1200, 800 };
        camera.zoom = 1.5F;
        canvas.scale = 1.25F;
        require(camera.createView({ 0, 0, 1000, 700 }, world, cameraError)
            && canvas.createView({ 0, 0, 1000, 700 }, view, error), "Independent views failed.");
        const Vector2 worldPixel = world.worldToScreen({ 1300, 850 });
        const Vector2 uiPixel = view.uiToScreen({ 100, 50 });
        canvas.scale = 2;
        require(canvas.createView({ 0, 0, 1000, 700 }, view, error)
            && near(world.worldToScreen({ 1300, 850 }), worldPixel), "UI scale changed world mapping.");
        canvas.scale = 1.25F;
        camera.zoom = 2.5F;
        require(camera.createView({ 0, 0, 1000, 700 }, world, cameraError)
            && canvas.createView({ 0, 0, 1000, 700 }, view, error)
            && near(view.uiToScreen({ 100, 50 }), uiPixel), "World zoom changed UI mapping.");
        require(canvas.createView({ 0, 0, 1400, 900 }, view, error)
            && canvas.scale == 1.25F && near(view.uiToScreen({ 100, 50 }), uiPixel)
            && view.size == Size2{ 1120, 720 }, "Resize changed chosen UI scale.");
        require(view.screenToUi({ 125, 62.5F }, point) && near(point, { 100, 50 }), "Mouse after resize failed.");

        const UiView saved = view;
        const auto unchanged = [&] {
            return view.viewport.width == saved.viewport.width && view.size == saved.size
                && near(view.uiToScreen({ 100, 50 }), saved.uiToScreen({ 100, 50 }));
        };
        for (const Viewport invalid : { Viewport{ 0, 0, 0, 600 }, Viewport{ 0, 0, 800, -1 },
            Viewport{ std::numeric_limits<int>::max(), 0, 100, 100 } }) {
            require(!canvas.createView(invalid, view, error) && error == UiError::InvalidViewport && unchanged(), "Invalid viewport changed output.");
        }
        for (float invalid : { 0.0F, -1.0F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() }) {
            canvas.scale = invalid;
            require(!canvas.createView({ 0, 0, 800, 600 }, view, error) && error == UiError::InvalidScale && unchanged(), "Invalid scale changed output.");
        }
        canvas.scale = std::numeric_limits<float>::min();
        require(!canvas.createView({ 0, 0, 800, 600 }, view, error) && error == UiError::NonInvertible && unchanged(), "Unrepresentable UI changed output.");
        canvas.scale = 1;
        require(canvas.createView({ 0, 0, 1, 1 }, view, error) && error == UiError::None
            && view.size == Size2{ 1, 1 }, "UI failed to recover.");
        require(!UiView{}.screenToUi({}, point), "Uninitialized UI accepted input.");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
