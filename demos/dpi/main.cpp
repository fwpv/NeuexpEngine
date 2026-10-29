#include "entry_point.h"
#include "application.h"
#include "ui.h"
#include "renderer.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using namespace Engine;

constexpr Color Grid = {0.17F, 0.25F, 0.28F};
constexpr Color Panel = {0.035F, 0.055F, 0.08F};
constexpr Color White = {0.91F, 0.96F, 1};
constexpr Color Orange = {1, 0.65F, 0.28F};
constexpr Color Blue = {0.4F, 0.86F, 1};
constexpr Rect UiPanel = {12, 12, 232, 152};
constexpr Rect UiMinus = {80, 44, 24, 24};
constexpr Rect UiPlus = {110, 44, 24, 24};
constexpr Rect UiSystem = {144, 44, 90, 24};
constexpr Rect ZoomMinus = {80, 76, 24, 24};
constexpr Rect ZoomPlus = {110, 76, 24, 24};
constexpr Rect ZoomReset = {144, 76, 90, 24};
constexpr Rect ClickButton = {20, 108, 214, 24};
constexpr float ScaleStep = 0.25F;
constexpr float MinimumScale = 0.5F;
constexpr float MaximumScale = 2.5F;

int percent(float scale)
{
    return static_cast<int>(std::lround(scale * 100));
}

class DpiApplication final : public Application
{
private:
    bool onStart(Platform::Error& error) override
    {
        Platform::WindowState state;
        if (!Platform::getWindowState(state, error)) {
            return false;
        }
        canvas.scale = std::clamp(state.systemScale, MinimumScale, MaximumScale);
        return initializeRenderer(error);
    }

    bool updateViews(const FrameInfo& frame, Platform::Error& error)
    {
        viewsReady = false;
        if (frame.window.width <= 0 || frame.window.height <= 0) {
            return true;
        }
        const Viewport area = {0, 0, frame.window.width, frame.window.height};
        CameraError cameraError;
        UiError uiError;
        if (!camera.createView(area, worldView, cameraError) || !canvas.createView(area, uiView, uiError)) {
            error = Platform::Error{"DPI demo views", "Could not calculate the world and interface coordinates.", std::optional<int>{}};
            return false;
        }
        viewsReady = true;
        return true;
    }

    void changeUiScale(float value)
    {
        canvas.scale = std::clamp(value, MinimumScale, MaximumScale);
        titleDirty = true;
    }

    bool onInput(const FrameInfo& frame, Platform::Error& error) override
    {
        const auto& input = Platform::getInput();
        if (input.key(Key::Escape).pressed) {
            requestExit();
        }
        if (input.key(Key::Minus).pressed) {
            changeUiScale(canvas.scale - ScaleStep);
        }
        if (input.key(Key::Equal).pressed) {
            changeUiScale(canvas.scale + ScaleStep);
        }
        if (input.key(Key::Digit0).pressed) {
            changeUiScale(frame.window.systemScale);
        }
        if (input.key(Key::R).pressed) {
            camera.position = Vector2{};
            camera.zoom = 1;
            titleDirty = true;
        }
        if (input.mouse().inside && input.mouse().wheelY != 0) {
            camera.zoom = std::clamp(camera.zoom + static_cast<float>(input.mouse().wheelY) * 0.1F, MinimumScale, MaximumScale);
            titleDirty = true;
        }
        if (!updateViews(frame, error)) {
            return false;
        }
        for (const Event& event : Platform::getEvents()) {
            if (event.type == EventType::WindowDpiChanged || event.type == EventType::WindowResized) {
                titleDirty = true;
            }
            if (!viewsReady || event.type != EventType::MousePressed || event.button != MouseButton::Left) {
                continue;
            }
            const Vector2 screen = {static_cast<float>(event.x), static_cast<float>(event.y)};
            Vector2 ui;
            if (uiView.screenToUi(screen, ui) && contains(UiPanel, ui)) {
                if (contains(UiMinus, ui)) {
                    changeUiScale(canvas.scale - ScaleStep);
                } else if (contains(UiPlus, ui)) {
                    changeUiScale(canvas.scale + ScaleStep);
                } else if (contains(UiSystem, ui)) {
                    changeUiScale(frame.window.systemScale);
                } else if (contains(ZoomMinus, ui)) {
                    camera.zoom = std::max(MinimumScale, camera.zoom - ScaleStep);
                } else if (contains(ZoomPlus, ui)) {
                    camera.zoom = std::min(MaximumScale, camera.zoom + ScaleStep);
                } else if (contains(ZoomReset, ui)) {
                    camera.zoom = 1;
                } else if (contains(ClickButton, ui)) {
                    ++uiClicks;
                }
            } else if (worldView.screenToWorld(screen, marker)) {
                ++worldClicks;
            }
            titleDirty = true;
            if (!updateViews(frame, error)) {
                return false;
            }
        }
        return true;
    }

    bool onUpdate(const FrameInfo& frame, Platform::Error&) override
    {
        Vector2 direction;
        if (Platform::isKeyDown(Key::A)) { direction.x -= 1; }
        if (Platform::isKeyDown(Key::D)) { direction.x += 1; }
        if (Platform::isKeyDown(Key::W)) { direction.y -= 1; }
        if (Platform::isKeyDown(Key::S)) { direction.y += 1; }
        camera.position += normalized(direction) * (260.0f * frame.deltaTime);
        return true;
    }

    bool onPrepareFrame(const FrameInfo& frame, Platform::Error& error) override
    {
        if (!updateViews(frame, error)) {
            return false;
        }
        if (titleDirty) {
            std::ostringstream title;
            title << "DPI | UI=" << percent(canvas.scale) << " DPI=" << percent(frame.window.systemScale)
                << " WORLD=" << percent(camera.zoom) << " UI-clicks=" << uiClicks << " world-clicks=" << worldClicks
                << " marker=" << std::lround(marker.x) << ',' << std::lround(marker.y)
                << " client=" << frame.window.width << 'x' << frame.window.height
                << " | -/+: UI, 0: system DPI, WASD: camera, wheel: zoom, R: reset camera";
            if (!Platform::setWindowTitle(title.str(), error)) {
                return false;
            }
            titleDirty = false;
        }
        return true;
    }

    void button(const Rect& bounds, std::string_view label, Vector2 mouse)
    {
        const bool hover = contains(bounds, mouse);
        drawing.drawRectangle(bounds, hover ? Blue : Grid);
        drawing.drawDebugText(label, bounds.position + Vector2{7, 7}, hover ? Panel : White);
    }

    bool onRender(const FrameInfo& frame, Platform::Error& error) override
    {
        if (!viewsReady) {
            return true;
        }
        beginFrame(frame.window.width, frame.window.height, Color{0.08F, 0.13F, 0.16F});
        drawing.setWorldView(worldView);
        const Rect& bounds = worldView.worldBounds;
        constexpr float GridSpacing = 100;
        const float firstX = std::floor(bounds.position.x / GridSpacing) * GridSpacing;
        const float firstY = std::floor(bounds.position.y / GridSpacing) * GridSpacing;
        for (float x = firstX; x <= bounds.position.x + bounds.size.width; x += GridSpacing) {
            drawing.drawRectangle(Rect{x, bounds.position.y, 1, bounds.size.height}, Grid);
        }
        for (float y = firstY; y <= bounds.position.y + bounds.size.height; y += GridSpacing) {
            drawing.drawRectangle(Rect{bounds.position.x, y, bounds.size.width, 1}, Grid);
        }
        drawing.drawRectangle(Rect{-160, -90, 100, 160}, Blue);
        drawing.drawRectangle(Rect{150, -180, 170, 80}, Color{0.35F, 0.95F, 0.55F});
        drawing.drawCircle(Circle{Vector2{}, 40}, Orange, Engine::ShapeFill::Filled, 40);
        drawing.drawCircle(Circle{marker, 13}, White, Engine::ShapeFill::Filled, 40);
        drawing.drawCircle(Circle{marker, 8}, Orange, Engine::ShapeFill::Filled, 40);

        drawing.setUiView(uiView);
        drawing.drawRectangle(UiPanel, Panel);
        drawing.drawDebugText("UI " + std::to_string(percent(canvas.scale)) + " / DPI " + std::to_string(percent(frame.window.systemScale)), Vector2{20, 22}, White);
        drawing.drawDebugText("UI", Vector2{20, 51}, Orange);
        drawing.drawDebugText("WORLD " + std::to_string(percent(camera.zoom)), Vector2{20, 83}, Blue, 1.5F);
        const auto& mouse = Platform::getInput().mouse();
        Vector2 pointer = {-1, -1};
        if (mouse.inside) {
            uiView.screenToUi(Vector2{static_cast<float>(mouse.x), static_cast<float>(mouse.y)}, pointer);
        }
        button(UiMinus, "-", pointer);
        button(UiPlus, "+", pointer);
        button(UiSystem, "USE DPI", pointer);
        button(ZoomMinus, "-", pointer);
        button(ZoomPlus, "+", pointer);
        button(ZoomReset, "RESET", pointer);
        button(ClickButton, "UI CLICKS " + std::to_string(uiClicks), pointer);
        drawing.drawDebugText("WASD / WHEEL / CLICK WORLD", Vector2{20, 146}, White, 1.5F);
        return endFrame(error);
    }

    Engine::Renderer2D& drawing = getRenderer();
    Camera2 camera = {Vector2{}, 1, Size2{960, 600}, ViewportMode::Expand};
    UiCanvas canvas;
    CameraView worldView;
    UiView uiView;
    bool viewsReady = false;
    bool titleDirty = true;
    int uiClicks = 0;
    int worldClicks = 0;
    Vector2 marker = {100, 80};
};
}

int Engine::applicationMain()
{
    ApplicationOptions options;
    options.window.title = "DPI";
    options.window.width = 1100;
    options.window.height = 720;
    options.window.minimumWidth = 640;
    options.window.minimumHeight = 480;
    DpiApplication application;
    std::vector<Platform::Error> errors;
    const bool success = application.run(options, errors);
    for (const auto& error : errors) {
        Platform::reportError(error);
    }
    return success ? 0 : 1;
}
