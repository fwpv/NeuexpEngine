#include "entry_point.h"
#include "platform.h"
#include "events.h"
#include "application.h"
#include "camera.h"
#include "geometry.h"
#include "input.h"
#include "math2d.h"
#include "renderer.h"
#include "ui.h"

#include <array>
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using Engine::Color;

class InputApplication final : public Engine::Application
{
private:
    bool onStart(Platform::Error& error) override
    {
        return initializeRenderer(error);
    }

    bool onInput(const Engine::FrameInfo& frame, Platform::Error&) override
    {
        const auto& input = Platform::getInput();
        for (const auto& event : Platform::getEvents()) {
            events.dispatch(event);
        }
        // Вспышки используют реальные часы, а переходы сохраняются
        // даже в кадре без отрисовки.
        constexpr double FlashDurationSeconds = 0.2;
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
            if (!Engine::UiCanvas{}.createView(Engine::Viewport{0, 0, window.width, window.height}, view, uiError)) {
                error = Platform::Error{"UiCanvas::createView",
                    "Could not calculate the input view. UiError: " + std::to_string(static_cast<int>(uiError)), std::nullopt};
                return false;
            }
            beginFrame(frame.window.width, frame.window.height, Color{0.04F, 0.05F, 0.08F});
            drawing.setUiView(view);
            constexpr int Columns = 16;
            constexpr float GridLeft = 20;
            constexpr float CellStep = 48;
            constexpr float CellSize = 40;
            for (std::size_t index = 1; index < pressedUntil.size(); ++index) {
                const auto state = input.key(static_cast<Engine::Key>(index));
                const float x = GridLeft + static_cast<float>((index - 1) % Columns) * CellStep;
                const float y = 20.0f + static_cast<float>((index - 1) / Columns) * CellStep;
                if (releasedUntil[index] > now) {
                    drawing.drawRectangle(Engine::Rect{x, y, CellSize, CellSize}, Color{1, 0.5F, 0.1F});
                } else if (pressedUntil[index] > now) {
                    drawing.drawRectangle(Engine::Rect{x, y, CellSize, CellSize}, Color{0.2F, 1, 0.3F});
                } else {
                    drawing.drawRectangle(Engine::Rect{x, y, CellSize, CellSize}, state.down ? Color{0.15F, 0.25F, 1} : Color{0.15F, 0.25F, 0.35F});
                }
            }
            for (int index = 1; index < static_cast<int>(Engine::MouseButton::Count); ++index) {
                const auto state = input.button(static_cast<Engine::MouseButton>(index));
                const Color color = {state.released ? 1.0F : 0.1F,
                    state.pressed ? 1.0F : 0.3F, state.down ? 1.0F : 0.4F};
                drawing.drawRectangle(Engine::Rect{GridLeft + static_cast<float>(index - 1) * CellStep, 420.0f, CellSize, CellSize}, color);
            }
            constexpr float CursorSize = 8;
            drawing.drawRectangle(Engine::Rect{static_cast<float>(mouse.x) - CursorSize / 2, static_cast<float>(mouse.y) - CursorSize / 2,
                CursorSize, CursorSize}, Color{1, 1, 1});
            return endFrame(error);
        }
        return true;
    }

    Engine::Renderer2D& drawing = getRenderer();
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
};
}

int Engine::applicationMain()
{
    Engine::ApplicationOptions options;
    options.window.title = "Input demo";
    options.window.width = 900;
    options.window.height = 500;
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
