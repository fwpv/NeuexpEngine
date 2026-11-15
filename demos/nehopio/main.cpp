#include "application.h"
#include "assets.h"
#include "audio.h"
#include "entry_point.h"
#include "game.h"
#include "input.h"
#include "platform.h"
#include "resources.h"

#include <optional>

namespace
{
constexpr char WindowTitle[] = "Nehopio";

class NehopioApplication final : public Engine::Application
{
private:
    bool onStart(Platform::Error& error) override
    {
        if (!initializeRenderer(error)) {
            return false;
        }
        Engine::ResourceFiles files;
        Engine::ResourceError failure;
        if (!files.initialize(failure) || !assets.load(getRenderer(), files, failure)
            || !game.load(assets, files, failure)) {
            error = Platform::Error{WindowTitle, failure.message, std::optional<int>{}};
            return false;
        }
        if (!audio.load(files, failure)) {
            error = Platform::Error{WindowTitle, failure.message, std::optional<int>{}};
            return false;
        }
        return true;
    }

    bool onInput(const Engine::FrameInfo&, Platform::Error&) override
    {
        // Одноразовые действия читаются из снимка ввода: флаг pressed действует один кадр
        // и не зависит от того, успел ли в этом кадре начаться фиксированный шаг симуляции.
        // Обработка находится здесь, потому что обновление на паузе не вызывается.
        const auto& input = Platform::getInput();
        if (input.key(Engine::Key::Escape).pressed) {
            requestExit();
        }
        if (input.key(Engine::Key::R).pressed) {
            game.restart();
        }
        if (input.key(Engine::Key::P).pressed) {
            setPaused(!isPaused());
        }
        return true;
    }

    bool onUpdate(const Engine::FrameInfo& frame, Platform::Error& error) override
    {
        const auto& input = Platform::getInput();
        Nehopio::Controls controls;
        controls.left = input.key(Engine::Key::ArrowLeft).down || input.key(Engine::Key::A).down;
        controls.right = input.key(Engine::Key::ArrowRight).down || input.key(Engine::Key::D).down;
        controls.jump = input.key(Engine::Key::Space).pressed || input.key(Engine::Key::ArrowUp).pressed
            || input.key(Engine::Key::W).pressed;
        controls.jumpHeld = input.key(Engine::Key::Space).down || input.key(Engine::Key::ArrowUp).down
            || input.key(Engine::Key::W).down;
        Engine::ResourceError failure;
        if (!game.update(frame.deltaTime, controls, failure)) {
            error = Platform::Error{WindowTitle, failure.message, std::optional<int>{}};
            return false;
        }
        // Звуки относятся к текущему обновлению: список очищается в начале следующего.
        for (const Nehopio::Sound sound : game.sounds()) {
            audio.play(sound);
        }
        return true;
    }

    bool onPrepareFrame(const Engine::FrameInfo& frame, Platform::Error& error) override
    {
        if (frame.window.minimized) {
            return true;
        }
        Engine::CameraError cameraError;
        if (!game.prepareView(frame.window.width, frame.window.height, cameraError)) {
            error = Platform::Error{WindowTitle, "Cannot calculate the camera view.", std::optional<int>{}};
            return false;
        }
        return true;
    }

    bool onRender(const Engine::FrameInfo& frame, Platform::Error& error) override
    {
        beginFrame(frame.window.width, frame.window.height, Engine::Color{0.33F, 0.62F, 0.93F});
        Engine::ResourceError failure;
        if (!game.draw(getRenderer(), assets, frame.paused, failure)) {
            error = Platform::Error{WindowTitle, failure.message, std::optional<int>{}};
            return false;
        }
        return endFrame(error);
    }

    bool onStop(Platform::Error&) override
    {
        audio.shutdown();
        return true;
    }

    Nehopio::Assets assets;
    Nehopio::Game game;
    Nehopio::Audio audio;
};
}

int Engine::applicationMain()
{
    NehopioApplication application;
    Engine::ApplicationOptions options;
    options.window.width = 1280;
    options.window.height = 720;
    options.window.minimumWidth = 960;
    options.window.minimumHeight = 540;
    options.window.title = WindowTitle;
    options.maxDeltaTime = 0.1F;
    options.targetFramesPerSecond = 120;
    Platform::Error error;
    if (!application.run(options, error)) {
        Platform::reportError(error);
        return 1;
    }
    return 0;
}
