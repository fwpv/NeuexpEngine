#include "game.h"
#include "entry_point.h"
#include "application.h"
#include "entities.h"
#include "input.h"
#include "math2d.h"
#include "platform.h"
#include "scene.h"
#include "renderer.h"
#include "camera.h"
#include "ui.h"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace
{
const std::string WindowTitle = "ORBITAL // SECTOR 09";

Game::InputState readInput()
{
    return Game::InputState{
        Platform::isKeyDown(Platform::Key::ArrowUp) || Platform::isKeyDown(Platform::Key::W),
        Platform::isKeyDown(Platform::Key::ArrowDown) || Platform::isKeyDown(Platform::Key::S),
        Platform::isKeyDown(Platform::Key::ArrowLeft) || Platform::isKeyDown(Platform::Key::A),
        Platform::isKeyDown(Platform::Key::ArrowRight) || Platform::isKeyDown(Platform::Key::D),
        Platform::isKeyDown(Platform::Key::Space)
    };
}

class CircleApplication final : public Engine::Application
{
private:
    bool onStart(Platform::Error& error) override
    {
        return initializeRenderer(error);
    }

    bool onInput(const Engine::FrameInfo&, Platform::Error&) override
    {
        if (Platform::isKeyDown(Platform::Key::Escape)) {
            requestExit();
        }
        if (Platform::isKeyPressed(Platform::Key::P)) {
            setPaused(!isPaused());
            titleDirty = true;
        }
        if (Platform::isKeyPressed(Platform::Key::V)) {
            const bool enabled = !verticalSync.value_or(false);
            Platform::Error error;
            verticalSyncFailed = !setVSync(enabled, error);
            if (verticalSyncFailed) {
                Platform::reportError(error, false);
            } else {
                verticalSync = enabled;
            }
            titleDirty = true;
        }
        return true;
    }

    bool onUpdate(const Engine::FrameInfo& frame, Platform::Error&) override
    {
        scene.update(readInput(), static_cast<float>(frame.elapsedTime), frame.deltaTime);
        return true;
    }

    bool onPrepareFrame(const Engine::FrameInfo& frame, Platform::Error&) override
    {
        if (frame.fpsUpdated || titleDirty) {
            const int fps = static_cast<int>(std::round(frame.framesPerSecond));
            const std::string syncStatus = verticalSyncFailed ? "error" :
                (verticalSync.has_value() ? (*verticalSync ? "on" : "off") : "driver");
            const std::string title = WindowTitle + " | FPS: " + std::to_string(fps) +
                (frame.paused ? " | PAUSED" : "") + " | VSync: " + syncStatus + " | P: pause | V: VSync";
            Platform::Error error;
            if (!Platform::setWindowTitle(title, error)) {
                Platform::reportError(error, false);
            }
            titleDirty = false;
        }
        return true;
    }

    bool onRender(const Engine::FrameInfo& frame, Platform::Error& error) override
    {
        const Engine::Size2 worldSize = scene.size();
        const Engine::Camera2 camera = {Engine::Vector2{worldSize.width / 2, worldSize.height / 2},
            1, worldSize, Engine::ViewportMode::Fit};
        Engine::CameraView worldView;
        Engine::UiView hudView;
        Engine::CameraError cameraError;
        if (!camera.createView(Engine::Viewport{0, 0, frame.window.width, frame.window.height}, worldView, cameraError)) {
            error = Platform::Error{"Camera2::createView",
                "Could not calculate the world view. CameraError: " + std::to_string(static_cast<int>(cameraError)),
                std::nullopt};
            return false;
        }
        Engine::UiError uiError;
        if (!Engine::UiCanvas{static_cast<float>(worldView.viewport.width) / worldSize.width}
                .createView(worldView.viewport, hudView, uiError)) {
            error = Platform::Error{"UiCanvas::createView",
                "Could not calculate the interface view. UiError: " + std::to_string(static_cast<int>(uiError)),
                std::nullopt};
            return false;
        }

        const float time = static_cast<float>(frame.elapsedTime);
        Engine::Renderer2D& drawing = getRenderer();
        beginFrame(frame.window.width, frame.window.height, Engine::Color{0.004f, 0.008f, 0.025f, 1.0f});
        drawing.setWorldView(worldView);
        scene.drawWorld(drawing, time);
        drawing.setUiView(hudView);
        scene.drawHud(drawing, time);
        return endFrame(error);
    }

    Game::Scene scene;
    bool titleDirty = true;
    std::optional<bool> verticalSync;
    bool verticalSyncFailed = false;
};
}

int Game::run()
{
    Engine::ApplicationOptions options;
    options.window.width = 800;
    options.window.height = 600;
    options.window.resizable = false;
    options.window.title = WindowTitle;
    options.maxDeltaTime = 0.08f;
    options.targetFramesPerSecond = 120.0;
    CircleApplication application;
    std::vector<Platform::Error> errors;
    const bool success = application.run(options, errors);
    bool showDialog = true;
    for (const Platform::Error& error : errors) {
        Platform::reportError(error, showDialog);
        showDialog = false;
    }
    return success ? 0 : 1;
}

int Engine::applicationMain()
{
    return Game::run();
}
