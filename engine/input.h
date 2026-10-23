#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Engine
{
// Физические позиции клавиш, названные по US-клавиатуре. Раскладка влияет только на TextInput.
enum class Key
{
    Unknown,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,
    Escape, Enter, Tab, Backspace, Space,
    Minus, Equal, LeftBracket, RightBracket, Backslash, Semicolon, Apostrophe, Grave,
    Comma, Period, Slash, InternationalBackslash,
    LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt, LeftSuper, RightSuper,
    Menu, CapsLock, NumLock, ScrollLock, PrintScreen, Pause,
    Insert, Delete, Home, End, PageUp, PageDown, ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
    NumpadDecimal, NumpadDivide, NumpadMultiply, NumpadSubtract, NumpadAdd, NumpadEnter,
    Count
};

enum class MouseButton
{
    Unknown, Left, Right, Middle, Back, Forward, Count
};

struct Modifiers
{
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool super = false;
    bool capsLock = false;
    bool numLock = false;
};

enum class EventType
{
    KeyPressed, KeyReleased, TextInput,
    MouseMoved, MousePressed, MouseReleased, MouseWheel, MouseEntered, MouseLeft,
    MouseCaptureLost, FocusGained, FocusLost, WindowResized, WindowMinimized,
    WindowRestored, CloseRequested, WindowDestroyed,
    LockStateChanged, // Изменились capsLock/numLock; остальные поля modifiers не используются.
    WindowDpiChanged
};

// Поля читаются согласно type; остальные имеют нулевое значение.
// x/y — фактические пиксели от левого верхнего угла клиентской области, +y вниз.
// При перетаскивании координаты могут выходить за границы окна; они не ограничиваются.
struct Event
{
    EventType type = EventType::MouseMoved;
    Key key = Key::Unknown;
    MouseButton button = MouseButton::Unknown;
    Modifiers modifiers;
    bool repeat = false; // Только KeyPressed; повтор не устанавливает pressed заново.
    bool canceled = false; // Синтетическое отпускание при потере фокуса/захвата.
    int x = 0;
    int y = 0;
    int deltaX = 0;
    int deltaY = 0;
    double wheelX = 0; // Шаги колеса: положительные значения вправо/вверх, возможны доли.
    double wheelY = 0;
    int width = 0;
    int height = 0;
    float systemScale = 0; // Только WindowDpiChanged: новый системный DPI / 96.
    std::string text; // Подтверждённый текст UTF-8, отдельно от управляющих клавиш.
    double time = 0; // Монотонное время получения события движком, в секундах.
};

struct ButtonState
{
    bool down = false;
    bool pressed = false;
    bool released = false;
};

struct MouseState
{
    int x = 0;
    int y = 0;
    int deltaX = 0;
    int deltaY = 0;
    double wheelX = 0;
    double wheelY = 0;
    bool inside = false;
};

// Накопитель событий и снимок одного кадра. Используется платформой; пригоден также
// для воспроизведения записанного ввода. beginFrame очищает только временные данные.
// handleEvent сохраняет порядок; чтение ничего не потребляет и не вызывает ОС.
class InputState
{
public:
    void beginFrame();
    void handleEvent(Event event);
    ButtonState key(Key key) const;
    ButtonState button(MouseButton button) const;
    const MouseState& mouse() const;
    const Modifiers& modifiers() const;
    const std::vector<Event>& events() const;
    const std::string& text() const;
    bool focused() const;

private:
    void releaseButtons(const Event& cause, bool keyboard);
    std::array<ButtonState, static_cast<std::size_t>(Key::Count)> keys = {};
    std::array<ButtonState, static_cast<std::size_t>(MouseButton::Count)> buttons = {};
    MouseState mouseState;
    Modifiers modifierState;
    std::vector<Event> frameEvents;
    std::string frameText;
    bool hasFocus = false;
    bool hasMousePosition = false;
    int motionX = 0;
    int motionY = 0;
};
}
