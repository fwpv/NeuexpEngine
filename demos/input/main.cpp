#include "entry_point.h"
#include "platform.h"
#include "events.h"
#include "application.h"
#include "math2d.h"
#include "../common/painter.h"

#include <array>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using Engine::Color;

constexpr int WindowWidth = 900;
constexpr int WindowHeight = 500;
constexpr int Columns = 16;
constexpr float GridLeft = 20;
constexpr float GridTop = 20;
constexpr float MouseRowTop = 420;
constexpr float CellStep = 48;
constexpr float CellSize = 40;
constexpr float CursorSize = 8;
constexpr float CursorHalfSize = CursorSize / 2;

constexpr Color BackgroundColor = { 0.04F, 0.05F, 0.08F };
constexpr Color KeyReleasedColor = { 1, 0.5F, 0.1F };
constexpr Color KeyPressedColor = { 0.2F, 1, 0.3F };
constexpr Color KeyHeldColor = { 0.15F, 0.25F, 1 };
constexpr Color KeyIdleColor = { 0.15F, 0.25F, 0.35F };
constexpr Color MouseActiveColor = { 1, 1, 1 };
constexpr Color MouseIdleColor = { 0.1F, 0.3F, 0.4F };
constexpr Color CursorColor = { 1, 1, 1 };

constexpr double FlashDurationSeconds = 0.2;

class InputApplication final : public Engine::Application
{
private:
    Demo::Painter drawing{ getRenderer() };

    bool onStart(Platform::Error& error) override
    {
        return drawing.initialize(error);
    }

    std::array<double, static_cast<std::size_t>(Engine::Key::Count)> pressedUntil = {};
    std::array<double, static_cast<std::size_t>(Engine::Key::Count)> releasedUntil = {};
    std::string text;
    int lastKey = 0;
    Engine::EventDispatcher events;
    // Подписки уничтожаются до данных, используемых обработчиками.
    Engine::EventSubscription keySubscription = events.subscribe([this](const Engine::Event& event) {
        if (event.type == Engine::EventType::KeyPressed || event.type == Engine::EventType::KeyReleased) {
            lastKey = static_cast<int>(event.key);
        }
    });
    Engine::EventSubscription textSubscription = events.subscribe(Engine::EventType::TextInput, [this](const Engine::Event& event) {
        text = event.text;
    });
    bool titleDirty = true;

    bool onInput(const Engine::FrameInfo& frame, Platform::Error&) override
    {
        const auto& input = Platform::getInput();
        for (const auto& event : Platform::getEvents()) {
            events.dispatch(event);
        }
        // Вспышки используют реальные часы, а переходы сохраняются
        // даже в кадре без отрисовки.
        for (std::size_t index = 1; index < pressedUntil.size(); ++index) {
            const auto state = input.key(static_cast<Engine::Key>(index));
            if (state.pressed) {
                pressedUntil[index] = frame.realTime + FlashDurationSeconds;
            }
            if (state.released) {
                releasedUntil[index] = frame.realTime + FlashDurationSeconds;
            }
        }
        titleDirty = titleDirty || !Platform::getEvents().empty();
        return true;
    }

    bool onPrepareFrame(const Engine::FrameInfo& frame, Platform::Error& error) override
    {
        const auto& input = Platform::getInput();
        const auto& mouse = input.mouse();
        if (titleDirty || frame.fpsUpdated) {
            std::ostringstream title;
            title << "Input | focus=" << input.focused() << " | key=" << lastKey
                << " | mouse=" << mouse.x << ',' << mouse.y << " delta=" << mouse.deltaX << ',' << mouse.deltaY
                << " | wheel=" << mouse.wheelX << ',' << mouse.wheelY << " | text=" << text
                << " | FPS=" << static_cast<int>(frame.framesPerSecond + 0.5);
            if (!Platform::setWindowTitle(title.str(), error)) {
                return false;
            }
            titleDirty = false;
        }
        return true;
    }

    bool onRender(const Engine::FrameInfo& frame, Platform::Error& error) override
    {
        const auto& window = frame.window;
        const auto& input = Platform::getInput();
        const auto& mouse = input.mouse();
        const double now = frame.realTime;
        if (window.width > 0 && window.height > 0) {
            Engine::UiView view;
            Engine::UiError uiError;
            if (!Engine::UiCanvas{}.createView({ 0, 0, window.width, window.height }, view, uiError)) {
                error = { "UiCanvas::createView", "Could not calculate the input view.", {} };
                return false;
            }
            drawing.beginFrame(frame, BackgroundColor);
            drawing.beginUi(view);
            for (std::size_t index = 1; index < pressedUntil.size(); ++index) {
                const auto state = input.key(static_cast<Engine::Key>(index));
                const float x = GridLeft + static_cast<float>((index - 1) % Columns) * CellStep;
                const float y = GridTop + static_cast<float>((index - 1) / Columns) * CellStep;
                if (releasedUntil[index] > now) {
                    drawing.rectangle({ x, y, CellSize, CellSize }, KeyReleasedColor);
                } else if (pressedUntil[index] > now) {
                    drawing.rectangle({ x, y, CellSize, CellSize }, KeyPressedColor);
                } else {
                    drawing.rectangle({ x, y, CellSize, CellSize }, state.down ? KeyHeldColor : KeyIdleColor);
                }
            }
            for (int index = 1; index < static_cast<int>(Engine::MouseButton::Count); ++index) {
                const auto state = input.button(static_cast<Engine::MouseButton>(index));
                const Color color = { state.released ? MouseActiveColor.red : MouseIdleColor.red,
                    state.pressed ? MouseActiveColor.green : MouseIdleColor.green,
                    state.down ? MouseActiveColor.blue : MouseIdleColor.blue };
                drawing.rectangle({ GridLeft + static_cast<float>(index - 1) * CellStep, MouseRowTop, CellSize, CellSize }, color);
            }
            drawing.rectangle({ static_cast<float>(mouse.x) - CursorHalfSize, static_cast<float>(mouse.y) - CursorHalfSize,
                CursorSize, CursorSize }, CursorColor);
            return drawing.endFrame(error);
        }
        return true;
    }
};
}

int Engine::applicationMain()
{
    Engine::ApplicationOptions options;
    options.window.title = "Input demo";
    options.window.width = WindowWidth;
    options.window.height = WindowHeight;
    options.targetFramesPerSecond = 100.0;
    InputApplication application;
    std::vector<Platform::Error> errors;
    const bool success = application.run(options, errors);
    bool showDialog = true;
    for (const auto& error : errors) {
        Platform::reportError(error, showDialog);
        showDialog = false;
    }
    return success ? 0 : 1;
}
