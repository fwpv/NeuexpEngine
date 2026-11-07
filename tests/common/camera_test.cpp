#include "camera.h"

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
        Camera2 camera;
        CameraView view;
        CameraError error = CameraError::InvalidCamera;
        require(camera.createView({ 0, 0, 800, 600 }, view, error) && error == CameraError::None, "Default view failed.");
        require(near(view.worldBounds.position, { -400, -300 }) && view.worldBounds.size == Size2{ 800, 600 }, "Default world bounds failed.");
        require(near(view.worldToScreen({}), { 400, 300 }), "Camera center mapping failed.");
        require(near(view.worldToScreen({ -400, -300 }), {}), "World corner mapping failed.");
        Vector2 world;
        require(view.screenToWorld({ 123, 456 }, world) && near(world, { -277, 156 }), "Mouse mapping failed.");

        camera.position = { 1000, -500 };
        camera.zoom = 2;
        require(camera.createView({ 40, 70, 800, 600 }, view, error), "Offset zoomed view failed.");
        require(near(view.worldToScreen(camera.position), { 440, 370 }), "Offset camera center failed.");
        require(near(view.worldBounds.size.width, 400) && near(view.worldBounds.size.height, 300), "Zoomed bounds failed.");
        require(view.screenToWorld({ 480, 390 }, world) && near(world, { 1020, -490 }), "Offset zoomed mouse failed.");
        require(near(view.worldToScreen(world), { 480, 390 }), "Mouse round trip failed.");

        camera = {};
        require(camera.createView({ 0, 0, 1200, 600 }, view, error), "Wide expand view failed.");
        require(near(view.worldBounds.size.width, 1200) && near(view.worldBounds.size.height, 600), "Wide expand policy failed.");
        require(camera.createView({ 0, 0, 600, 900 }, view, error), "Tall expand view failed.");
        require(near(view.worldBounds.size.width, 800) && near(view.worldBounds.size.height, 1200), "Tall expand policy failed.");

        camera.mode = ViewportMode::Fit;
        require(camera.createView({ 30, 40, 1200, 600 }, view, error), "Fit view failed.");
        require(view.viewport.x == 230 && view.viewport.y == 40 && view.viewport.width == 800 && view.viewport.height == 600,
            "Fit viewport alignment failed.");
        require(view.worldBounds.size == Size2{ 800, 600 }, "Fit world size failed.");
        world = { 17, 19 };
        require(!view.screenToWorld({ 100, 200 }, world) && world == Vector2{ 17, 19 }, "Letterbox accepted input or changed output.");
        require(view.screenToWorld({ 230, 40 }, world) && near(world, { -400, -300 }), "Fit origin rejected.");
        require(view.screenToWorld({ 1029, 639 }, world), "Last viewport pixel rejected.");
        require(!view.screenToWorld({ 1030, 300 }, world) && !view.screenToWorld({ 500, 640 }, world), "Half-open viewport boundary failed.");
        require(!view.screenToWorld({ 229, 40 }, world) && !view.screenToWorld({ 230, 39 }, world), "Viewport outer boundary failed.");

        require(camera.createView({ 0, 0, 800, 1000 }, view, error)
            && view.viewport.y == 200 && view.viewport.height == 600, "Horizontal letterbox failed.");
        camera.referenceSize = { 16, 9 };
        require(camera.createView({ 11, 13, 1001, 701 }, view, error), "Odd viewport failed.");
        require(view.viewport.x >= 11 && view.viewport.y >= 13
            && view.viewport.x + view.viewport.width <= 1012 && view.viewport.y + view.viewport.height <= 714,
            "Rounded viewport escaped its area.");
        require(near(view.worldToScreenTransform.m00, view.worldToScreenTransform.m11), "Viewport rounding stretched the world.");
        const Vector2 sample = { 2, -1 };
        require(view.screenToWorld(view.worldToScreen(sample), world) && near(world, sample), "Rounded mapping round trip failed.");

        Camera2 minimap;
        minimap.position = { 1500, 1000 };
        minimap.referenceSize = { 3000, 2000 };
        minimap.mode = ViewportMode::Fit;
        CameraView map;
        require(minimap.createView({ 700, 400, 300, 200 }, map, error), "Minimap view failed.");
        require(near(map.worldToScreen({ 1500, 1000 }), { 850, 500 }), "Minimap center failed.");
        require(map.screenToWorld({ 880, 520 }, world) && near(world, { 1800, 1200 }), "Minimap mouse failed.");
        camera = {};
        camera.position = world;
        camera.zoom = 1.5F;
        require(camera.createView({ 0, 0, 1000, 700 }, view, error)
            && near(view.worldToScreen(world), { 500, 350 }), "Camera follow failed.");
        require(camera.createView({ 0, 0, 1300, 800 }, view, error)
            && near(view.worldToScreen(world), { 650, 400 }), "Camera center after resize failed.");
        require(view.screenToWorld({ 650, 400 }, world) && near(world, camera.position), "Mouse after resize failed.");
        require(near(map.worldToScreen(camera.position), { 880, 520 }), "Main camera changed independent minimap.");

        const CameraView saved = view;
        const auto unchanged = [&] {
            return view.viewport.width == saved.viewport.width && view.worldBounds == saved.worldBounds
                && near(view.worldToScreen(camera.position), saved.worldToScreen(camera.position));
        };
        camera.mode = static_cast<ViewportMode>(-1);
        require(!camera.createView(Viewport{0, 0, 800, 600}, view, error)
            && error == CameraError::InvalidCamera && unchanged(), "Unknown viewport mode accepted or changed output.");
        camera.mode = ViewportMode::Expand;
        require(!camera.createView({ 0, 0, 0, 600 }, view, error) && error == CameraError::InvalidViewport && unchanged(), "Zero viewport accepted.");
        require(!camera.createView({ 0, 0, 800, -1 }, view, error) && error == CameraError::InvalidViewport && unchanged(), "Negative viewport accepted.");
        require(!camera.createView({ std::numeric_limits<int>::max(), 0, 100, 100 }, view, error)
            && error == CameraError::InvalidViewport, "Overflowing viewport accepted.");
        const float invalidValues[] = { 0, -1 };
        for (float invalid : invalidValues) {
            camera.zoom = invalid;
            require(!camera.createView({ 0, 0, 800, 600 }, view, error) && error == CameraError::InvalidCamera, "Invalid zoom accepted.");
            camera.zoom = 1;
            camera.referenceSize.width = invalid;
            require(!camera.createView({ 0, 0, 800, 600 }, view, error) && error == CameraError::InvalidCamera, "Invalid reference size accepted.");
            camera.referenceSize = { 800, 600 };
        }
        // Размер участвует в преобразовании к int, поэтому NaN/Inf здесь отклоняются.
        for (float invalid : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
            camera.mode = ViewportMode::Fit;
            camera.referenceSize.width = invalid;
            require(!camera.createView({0, 0, 800, 600}, view, error) && error == CameraError::InvalidCamera,
                "Unsafe viewport conversion accepted.");
        }
        camera = {};
        require(camera.createView({ 0, 0, 800, 600 }, view, error) && error == CameraError::None, "Camera did not recover after invalid settings.");
        require(!CameraView{}.screenToWorld({}, world), "Uninitialized view accepted input.");
        const CameraView valid = view;
        camera.zoom = std::numeric_limits<float>::min();
        require(!camera.createView({ 0, 0, 800, 600 }, view, error) && error == CameraError::NonInvertible
            && view.worldBounds == valid.worldBounds, "Unrepresentable camera mapping changed output.");
        camera = {};
        camera.mode = ViewportMode::Fit;
        require(camera.createView({ 0, 0, 1, 1 }, view, error) && error == CameraError::None
            && view.viewport.width == 1 && view.viewport.height == 1, "Smallest viewport failed.");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
