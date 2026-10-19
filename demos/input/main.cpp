#include "platform.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

#include <array>
#include <chrono>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
struct Color
{
    float red;
    float green;
    float blue;
};

// Размеры и расположение индикаторов в пикселях клиентской области.
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

// Цвета клавиш, отдельных каналов состояния кнопок мыши и курсора.
constexpr Color BackgroundColor = { 0.04F, 0.05F, 0.08F };
constexpr Color KeyReleasedColor = { 1, 0.5F, 0.1F };
constexpr Color KeyPressedColor = { 0.2F, 1, 0.3F };
constexpr Color KeyHeldColor = { 0.15F, 0.25F, 1 };
constexpr Color KeyIdleColor = { 0.15F, 0.25F, 0.35F };
constexpr Color MouseActiveColor = { 1, 1, 1 };
constexpr Color MouseIdleColor = { 0.1F, 0.3F, 0.4F };
constexpr Color CursorColor = { 1, 1, 1 };

// Вспышка позволяет заметить короткий переход между кадрами.
constexpr double FlashDurationSeconds = 0.2;
constexpr auto FrameDelay = std::chrono::milliseconds(10);

void rectangle(float x, float y, float width, float height, const Color& color)
{
    glColor3f(color.red, color.green, color.blue);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#else
int main()
#endif
{
    Platform::WindowOptions options;
    options.title = "Input demo";
    options.width = WindowWidth;
    options.height = WindowHeight;
    std::vector<Platform::Error> errors;
    if (!Platform::initialize(options, errors)) {
        for (const auto& error : errors) {
            Platform::reportError(error);
        }
        return 1;
    }
    std::array<double, static_cast<std::size_t>(Engine::Key::Count)> pressedUntil = {};
    std::array<double, static_cast<std::size_t>(Engine::Key::Count)> releasedUntil = {};
    std::string text;
    int lastKey = 0;
    bool success = true;
    Platform::Error error;
    while (success) {
        if (!Platform::processEvents(error)) {
            success = false;
            break;
        }
        if (Platform::isCloseRequested()) {
            break;
        }
        const auto& input = Platform::getInput();
        const auto& mouse = input.mouse();
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        for (const auto& event : Platform::getEvents()) {
            if (event.type == Engine::EventType::KeyPressed || event.type == Engine::EventType::KeyReleased) {
                lastKey = static_cast<int>(event.key);
            }
            if (event.type == Engine::EventType::TextInput) {
                text = event.text;
            }
        }
        if (!Platform::getEvents().empty()) {
            std::ostringstream title;
            title << "Input | focus=" << input.focused() << " | key=" << lastKey
                << " | mouse=" << mouse.x << ',' << mouse.y << " delta=" << mouse.deltaX << ',' << mouse.deltaY
                << " | wheel=" << mouse.wheelX << ',' << mouse.wheelY << " | text=" << text;
            if (!Platform::setWindowTitle(title.str(), error)) {
                success = false;
                break;
            }
        }
        Platform::WindowState window;
        if (!Platform::getWindowState(window, error)) {
            success = false;
            break;
        }
        if (window.width > 0 && window.height > 0 && !window.minimized) {
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0, window.width, window.height, 0, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glClearColor(BackgroundColor.red, BackgroundColor.green, BackgroundColor.blue, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            for (std::size_t index = 1; index < pressedUntil.size(); ++index) {
                const auto state = input.key(static_cast<Engine::Key>(index));
                if (state.pressed) { pressedUntil[index] = now + FlashDurationSeconds; }
                if (state.released) { releasedUntil[index] = now + FlashDurationSeconds; }
                const float x = GridLeft + static_cast<float>((index - 1) % Columns) * CellStep;
                const float y = GridTop + static_cast<float>((index - 1) / Columns) * CellStep;
                if (releasedUntil[index] > now) {
                    rectangle(x, y, CellSize, CellSize, KeyReleasedColor);
                } else if (pressedUntil[index] > now) {
                    rectangle(x, y, CellSize, CellSize, KeyPressedColor);
                } else {
                    rectangle(x, y, CellSize, CellSize, state.down ? KeyHeldColor : KeyIdleColor);
                }
            }
            for (int index = 1; index < static_cast<int>(Engine::MouseButton::Count); ++index) {
                const auto state = input.button(static_cast<Engine::MouseButton>(index));
                const Color color = { state.released ? MouseActiveColor.red : MouseIdleColor.red,
                    state.pressed ? MouseActiveColor.green : MouseIdleColor.green,
                    state.down ? MouseActiveColor.blue : MouseIdleColor.blue };
                rectangle(GridLeft + static_cast<float>(index - 1) * CellStep, MouseRowTop, CellSize, CellSize, color);
            }
            rectangle(static_cast<float>(mouse.x) - CursorHalfSize, static_cast<float>(mouse.y) - CursorHalfSize,
                CursorSize, CursorSize, CursorColor);
            if (!Platform::present(error)) {
                success = false;
                break;
            }
        }
        std::this_thread::sleep_for(FrameDelay);
    }
    if (!success) {
        Platform::reportError(error);
    }
    Platform::shutdown(errors);
    for (const auto& cleanupError : errors) {
        Platform::reportError(cleanupError);
    }
    return success && errors.empty() ? 0 : 1;
}
