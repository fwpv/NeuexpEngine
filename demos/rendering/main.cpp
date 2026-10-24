#include "application.h"
#include "renderer.h"
#include "../common/renderer_text.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using namespace Engine;

constexpr Color Background = { 0.025F, 0.04F, 0.07F };
constexpr Color Ground = { 0.055F, 0.09F, 0.14F };
constexpr Color Grid = { 0.12F, 0.19F, 0.26F };
constexpr Color White = { 0.91F, 0.96F, 1 };
constexpr Color Blue = { 0.25F, 0.75F, 1 };
constexpr Color Orange = { 1, 0.58F, 0.2F };
constexpr Color Green = { 0.35F, 0.95F, 0.65F };
constexpr Color Panel = { 0.025F, 0.04F, 0.07F, 0.94F };
constexpr float MinimumScale = 0.5F;
constexpr float MaximumScale = 2.5F;
constexpr float GridSpacing = 80;
const std::vector<Vector2> Concave = { { 0, 0 }, { 95, 0 }, { 95, 30 }, { 35, 30 }, { 35, 95 }, { 0, 95 } };

DrawOptions style(Color color, const Transform2& transform = {})
{
    DrawOptions options;
    options.color = color;
    options.transform = transform.toAffine();
    return options;
}

bool renderResult(bool success, RenderError renderError, Platform::Error& error)
{
    if (!success) {
        error = { "Renderer gallery", "Rendering failed (RenderError="
            + std::to_string(static_cast<int>(renderError)) + ").", {} };
    }
    return success;
}

class RenderingApplication final : public Application
{
    Renderer2D renderer;
    Camera2 camera = { {}, 1, { 1000, 700 }, ViewportMode::Fit };
    UiCanvas canvas;
    CameraView world;
    UiView ui;
    bool viewsReady = false;
    bool clipped = true;
    Vector2 marker;
    bool hasMarker = false;

    bool updateViews(const FrameInfo& frame, Platform::Error& error)
    {
        viewsReady = false;
        if (frame.window.width <= 0 || frame.window.height <= 0) {
            return true;
        }
        const Viewport area = { 0, 0, frame.window.width, frame.window.height };
        CameraError cameraError;
        UiError uiError;
        if (!camera.createView(area, world, cameraError) || !canvas.createView(area, ui, uiError)) {
            error = { "Renderer gallery", "Could not calculate world/UI views.", {} };
            return false;
        }
        viewsReady = true;
        return true;
    }

    bool onStart(Platform::Error& error) override
    {
        Platform::WindowState state;
        if (!Platform::getWindowState(state, error)) {
            return false;
        }
        canvas.scale = std::clamp(state.systemScale, MinimumScale, MaximumScale);
        return true;
    }

    bool onInput(const FrameInfo& frame, Platform::Error& error) override
    {
        const auto& input = Platform::getInput();
        if (input.key(Key::Escape).pressed) { requestExit(); }
        if (input.key(Key::P).pressed) { setPaused(!isPaused()); }
        if (input.key(Key::C).pressed) { clipped = !clipped; }
        if (input.key(Key::F).pressed) {
            camera.mode = camera.mode == ViewportMode::Fit ? ViewportMode::Expand : ViewportMode::Fit;
        }
        if (input.key(Key::Minus).pressed) { canvas.scale = std::max(MinimumScale, canvas.scale - 0.25F); }
        if (input.key(Key::Equal).pressed) { canvas.scale = std::min(MaximumScale, canvas.scale + 0.25F); }
        if (input.key(Key::R).pressed) {
            camera.position = {};
            camera.zoom = 1;
        }
        if (input.mouse().inside) {
            camera.zoom = std::clamp(camera.zoom + static_cast<float>(input.mouse().wheelY) * 0.1F,
                MinimumScale, MaximumScale);
        }
        if (!updateViews(frame, error)) {
            return false;
        }
        if (viewsReady && input.button(MouseButton::Left).pressed) {
            Vector2 uiPoint;
            const Vector2 pixel = { static_cast<float>(input.mouse().x), static_cast<float>(input.mouse().y) };
            const bool overUi = ui.screenToUi(pixel, uiPoint) && contains(Rect{ 12, 12, 460, 112 }, uiPoint);
            if (!overUi) {
                hasMarker = world.screenToWorld(pixel, marker);
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
        camera.position += normalized(direction) * (240 * frame.deltaTime);
        return true;
    }

    bool onPrepareFrame(const FrameInfo& frame, Platform::Error& error) override
    {
        if (!updateViews(frame, error)) {
            return false;
        }
        std::ostringstream title;
        title << "Renderer 05 | " << (camera.mode == ViewportMode::Fit ? "FIT" : "EXPAND")
            << " | zoom=" << camera.zoom << " UI=" << canvas.scale
            << " | " << (isPaused() ? "PAUSED" : "RUNNING")
            << " | WASD camera, wheel zoom, F fit/expand, C clip, -/+ UI, R reset, P pause, Esc exit";
        return Platform::setWindowTitle(title.str(), error);
    }

    bool drawWorld(float time, RenderError& error)
    {
        if (!renderer.beginWorld(world, error)
            || !renderer.drawRectangle(world.worldBounds, ShapeFill::Filled, style(Ground), error)) {
            return false;
        }
        const Rect bounds = world.worldBounds;
        for (float x = std::floor(bounds.position.x / GridSpacing) * GridSpacing;
            x < bounds.position.x + bounds.size.width; x += GridSpacing) {
            if (!renderer.drawLine({ x, bounds.position.y }, { x, bounds.position.y + bounds.size.height }, style(Grid), error)) {
                return false;
            }
        }
        for (float y = std::floor(bounds.position.y / GridSpacing) * GridSpacing;
            y < bounds.position.y + bounds.size.height; y += GridSpacing) {
            if (!renderer.drawLine({ bounds.position.x, y }, { bounds.position.x + bounds.size.width, y }, style(Grid), error)) {
                return false;
            }
        }
        DrawOptions point = style(White);
        point.pointSize = 7;
        const Transform2 spinning = { { -250, -85 }, time * 0.6F,
            { 1.1F + 0.25F * std::sin(time), 0.85F }, { 60, 40 } };
        if (!renderer.drawRectangle({ 0, 0, 120, 80 }, ShapeFill::Filled, style(Blue, spinning), error)
            || !renderer.drawRectangle({ 0, 0, 120, 80 }, ShapeFill::Outline, style(White, spinning), error)
            || !renderer.drawPoint(spinning.position, point, error)
            || !renderer.drawCircle({ { -25, -90 }, 54 }, ShapeFill::Filled, style(Orange), error)
            || !renderer.drawCircle({ { -25, -90 }, 70 }, ShapeFill::Outline, style(White), error)
            || !renderer.drawPolygon(Concave, ShapeFill::Filled,
                style(Green, { { 170, -130 }, 0.15F * std::sin(time), { 1.2F, 1.2F }, {} }), error)
            || !renderer.drawPolygon(Concave, ShapeFill::Outline,
                style(White, { { 310, -130 }, 0, { -0.8F, 0.8F }, {} }), error)
            || !renderer.drawLine({ -350, 25 }, { 350, 25 }, style(White), error)
            || !renderer.drawCircle({ { -220 + 22 * std::sin(time), 125 }, 64 }, ShapeFill::Filled,
                style({ Blue.red, Blue.green, Blue.blue, 0.6F }), error)
            || !renderer.drawCircle({ { -165, 125 }, 64 }, ShapeFill::Filled,
                style({ Orange.red, Orange.green, Orange.blue, 0.6F }), error)) {
            return false;
        }
        if (hasMarker && (!renderer.drawCircle({ marker, 12 }, ShapeFill::Outline, style(White), error)
            || !renderer.drawPoint(marker, point, error))) {
            return false;
        }
        return true;
    }

    bool drawUi(RenderError& error)
    {
        if (!renderer.beginUi(ui, error)
            || !renderer.drawRectangle({ 12, 12, 460, 112 }, ShapeFill::Filled, style(Panel), error)
            || !Demo::text(renderer, "RENDERER 05 - PRIMITIVES AND VIEWS", { 24, 24 }, White, error)
            || !Demo::text(renderer, "WASD CAMERA / WHEEL ZOOM / F FIT", { 24, 46 }, Blue, error)
            || !Demo::text(renderer, "C CLIP / -+ UI SCALE / R RESET", { 24, 68 }, Green, error)
            || !Demo::text(renderer, "P PAUSE / CLICK WORLD / ESC EXIT", { 24, 90 }, Orange, error)) {
            return false;
        }
        const Rect box = { 50, 160, 160, 86 };
        if (!renderer.drawRectangle(box, ShapeFill::Outline, style(White), error)) {
            return false;
        }
        const Vector2 clipStart = ui.uiToScreen(box.position);
        const Vector2 clipEnd = ui.uiToScreen(box.position + Vector2{ box.size.width, box.size.height });
        const Viewport clipArea = { static_cast<int>(std::ceil(clipStart.x)), static_cast<int>(std::ceil(clipStart.y)),
            static_cast<int>(std::floor(clipEnd.x) - std::ceil(clipStart.x)),
            static_cast<int>(std::floor(clipEnd.y) - std::ceil(clipStart.y)) };
        if (!renderer.setClip(clipped ? std::optional<Viewport>{ clipArea } : std::nullopt, error)
            || !renderer.drawCircle({ { 70, 200 }, 65 }, ShapeFill::Filled, style(Blue), error)
            || !renderer.drawCircle({ { 185, 200 }, 65 }, ShapeFill::Filled,
                style({ Green.red, Green.green, Green.blue, 0.7F }), error)
            || !renderer.setClip(std::nullopt, error)
            || !Demo::text(renderer, clipped ? "CLIP ON" : "CLIP OFF", { 50, 270 }, White, error)) {
            return false;
        }
        return true;
    }

    bool onRender(const FrameInfo& frame, Platform::Error& error) override
    {
        if (!viewsReady) {
            return true;
        }
        RenderError renderError;
        bool success = renderer.beginFrame(frame.window.width, frame.window.height, Background, renderError);
        if (success) {
            success = drawWorld(static_cast<float>(frame.elapsedTime), renderError) && drawUi(renderError);
            RenderError endError;
            const bool ended = renderer.endFrame(endError);
            if (success && !ended) {
                success = false;
                renderError = endError;
            }
        }
        return renderResult(success, renderError, error);
    }
};
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#else
int main()
#endif
{
    RenderingApplication application;
    ApplicationOptions options;
    options.window.width = 1100;
    options.window.height = 760;
    options.window.title = "Renderer gallery";
    options.window.minimumWidth = 520;
    options.window.minimumHeight = 360;
    std::vector<Platform::Error> errors;
    if (!application.run(options, errors)) {
        for (const auto& error : errors) {
            Platform::reportError(error);
        }
        return 1;
    }
    return 0;
}
