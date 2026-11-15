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

class NehopioApplication final : public NeuexpEngine::Application
{
private:
    bool onStart(Platform::Error& error) override
    {
        if (!initializeRenderer(error)) {
            return false;
        }
        NeuexpEngine::ResourceFiles files;
        NeuexpEngine::ResourceError failure;
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

    bool onInput(const NeuexpEngine::FrameInfo&, Platform::Error&) override
    {
        // Одноразовые действия читаются из снимка ввода: флаг pressed действует один кадр
        // и не зависит от того, успел ли в этом кадре начаться фиксированный шаг симуляции.
        // Обработка находится здесь, потому что обновление на паузе не вызывается.
        const auto& input = Platform::getInput();
        if (input.key(NeuexpEngine::Key::Escape).pressed) {
            requestExit();
        }
        if (input.key(NeuexpEngine::Key::R).pressed) {
            game.restart();
        }
        if (input.key(NeuexpEngine::Key::P).pressed) {
            setPaused(!isPaused());
        }
        return true;
    }

    bool onUpdate(const NeuexpEngine::FrameInfo& frame, Platform::Error& error) override
    {
        const auto& input = Platform::getInput();
        Nehopio::Controls controls;
        controls.left = input.key(NeuexpEngine::Key::ArrowLeft).down || input.key(NeuexpEngine::Key::A).down;
        controls.right = input.key(NeuexpEngine::Key::ArrowRight).down || input.key(NeuexpEngine::Key::D).down;
        controls.jump = input.key(NeuexpEngine::Key::Space).pressed || input.key(NeuexpEngine::Key::ArrowUp).pressed
            || input.key(NeuexpEngine::Key::W).pressed;
        controls.jumpHeld = input.key(NeuexpEngine::Key::Space).down || input.key(NeuexpEngine::Key::ArrowUp).down
            || input.key(NeuexpEngine::Key::W).down;
        NeuexpEngine::ResourceError failure;
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

    bool onPrepareFrame(const NeuexpEngine::FrameInfo& frame, Platform::Error& error) override
    {
        if (frame.window.minimized) {
            return true;
        }
        NeuexpEngine::CameraError cameraError;
        if (!game.prepareView(frame.window.width, frame.window.height, cameraError)) {
            error = Platform::Error{WindowTitle, "Cannot calculate the camera view.", std::optional<int>{}};
            return false;
        }
        return true;
    }

    bool onRender(const NeuexpEngine::FrameInfo& frame, Platform::Error& error) override
    {
        beginFrame(frame.window.width, frame.window.height, NeuexpEngine::Color{0.33F, 0.62F, 0.93F});
        NeuexpEngine::ResourceError failure;
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

int NeuexpEngine::applicationMain()
{
    NehopioApplication application;
    NeuexpEngine::ApplicationOptions options;
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
