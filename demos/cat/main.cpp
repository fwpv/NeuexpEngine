#include "application.h"
#include "entry_point.h"
#include "game.h"
#include "input.h"
#include "platform.h"
#include "renderer.h"
#include "resources.h"
#include "camera.h"

#include <cmath>
#include <optional>
#include <random>
#include <string>

namespace
{
namespace Host = ::Platform;

const std::string WindowTitle = "Cat-AI-rework";

Engine::ApplicationOptions applicationOptions()
{
    Engine::ApplicationOptions options;
    options.window.width = 935;
    options.window.height = 700;
    options.window.minimumWidth = 935;
    options.window.minimumHeight = 700;
    options.window.title = WindowTitle;
    options.maxDeltaTime = 0.2F;
    options.targetFramesPerSecond = 100;
    return options;
}

class CatApplication final : public Engine::Application
{
private:
    bool onStart(Host::Error& error) override;
    bool onInput(const Engine::FrameInfo& frame, Host::Error& error) override;
    bool onUpdate(const Engine::FrameInfo& frame, Host::Error&) override;
    bool onPrepareFrame(const Engine::FrameInfo& frame, Host::Error& error) override;
    bool onRender(const Engine::FrameInfo& frame, Host::Error& error) override;

    Cat::Game game{std::random_device{}(), std::random_device{}()};
    Cat::Images images;
    bool verticalSync = false;
    int displayedFps = -1;
};

bool CatApplication::onStart(Host::Error& error)
{
    if (!initializeRenderer(error)) {
        return false;
    }
    {
        Host::Error syncError;
        verticalSync = setVSync(true, syncError);
        if (!verticalSync) {
            Host::reportError(syncError, false);
        }
    }
    Engine::ResourceError resourceError;
    if (!images.load(getRenderer(), resourceError)) {
        error = Platform::Error{"cat", resourceError.message, std::optional<int>{}};
        return false;
    }
    return true;
}

bool CatApplication::onInput(const Engine::FrameInfo&, Host::Error&)
{
    if (Host::isKeyPressed(Engine::Key::Escape)) {
        requestExit();
    }
    if (Host::isKeyPressed(Engine::Key::V)) {
        Host::Error syncError;
        if (setVSync(!verticalSync, syncError)) {
            verticalSync = !verticalSync;
        } else {
            Host::reportError(syncError, false);
        }
    }
    return true;
}

bool CatApplication::onUpdate(const Engine::FrameInfo& frame, Host::Error&)
{
    const Cat::Controls controls{Host::isKeyDown(Engine::Key::ArrowRight),
        Host::isKeyDown(Engine::Key::ArrowLeft), Host::isKeyPressed(Engine::Key::ArrowUp),
        Host::isKeyPressed(Engine::Key::ArrowDown)};
    game.update(frame.deltaTime, controls);
    return true;
}

bool CatApplication::onPrepareFrame(const Engine::FrameInfo& frame, Host::Error& error)
{
    if (Engine::CameraError viewError; !frame.window.minimized
        && !game.prepareView(frame.window.width, frame.window.height, viewError)) {
        error = Platform::Error{"cat", "Cannot calculate game viewport.", std::optional<int>{}};
        return false;
    }
    const int fps = static_cast<int>(std::round(frame.framesPerSecond));
    if (fps != displayedFps) {
        if (!Host::setWindowTitle(WindowTitle + " | " + std::to_string(fps) + " FPS", error)) {
            return false;
        }
        displayedFps = fps;
    }
    return true;
}

bool CatApplication::onRender(const Engine::FrameInfo& frame, Host::Error& error)
{
    beginFrame(frame.window.width, frame.window.height, Engine::Color{245.0F / 255, 250.0F / 255, 245.0F / 255});
    game.draw(getRenderer(), images);
    return endFrame(error);
}
}

int Engine::applicationMain()
{
    CatApplication application;
    Platform::Error failure;
    const bool success = application.run(applicationOptions(), failure);
    if (!success) {
        Platform::reportError(failure);
    }
    return success ? 0 : 1;
}
