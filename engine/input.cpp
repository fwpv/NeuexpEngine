#include "input.h"

namespace Engine
{
namespace
{
template<class Enum, std::size_t Size>
ButtonState* findState(std::array<ButtonState, Size>& states, Enum value)
{
    const auto index = static_cast<std::size_t>(value);
    return index > 0 && index < Size ? &states[index] : nullptr;
}

void updateButton(ButtonState* state, bool down)
{
    if (state) {
        if (down && !state->down) {
            state->pressed = true;
        }
        if (!down && state->down) {
            state->released = true;
        }
        state->down = down;
    }
}
}

void InputState::beginFrame()
{
    for (auto& state : keys) {
        state.pressed = state.released = false;
    }
    for (auto& state : buttons) {
        state.pressed = state.released = false;
    }
    mouseState.deltaX = mouseState.deltaY = 0;
    mouseState.wheelX = mouseState.wheelY = 0;
    frameText.clear();
    frameEvents.clear();
}

void InputState::releaseButtons(const Event& cause, bool keyboard)
{
    // Отмена приходит после причины. GUI может отличить отмену drag от обычного отпускания.
    if (keyboard) {
        for (std::size_t index = 1; index < keys.size(); ++index) {
            if (keys[index].down) {
                Event release;
                release.type = EventType::KeyReleased;
                release.key = static_cast<Key>(index);
                release.canceled = true;
                release.time = cause.time;
                handleEvent(release);
            }
        }
    }
    for (std::size_t index = 1; index < buttons.size(); ++index) {
        if (buttons[index].down) {
            Event release;
            release.type = EventType::MouseReleased;
            release.button = static_cast<MouseButton>(index);
            release.x = mouseState.x;
            release.y = mouseState.y;
            release.canceled = true;
            release.time = cause.time;
            release.modifiers = modifierState;
            handleEvent(release);
        }
    }
}

void InputState::handleEvent(Event event)
{
    if (event.type == EventType::KeyPressed || event.type == EventType::KeyReleased) {
        auto* state = findState(keys, event.key);
        if (event.type == EventType::KeyPressed) {
            // Одинаковая семантика Win32/X11: повтор — новое сообщение уже удерживаемой
            // в этом окне клавиши. После отмены первое полученное сообщение начинает новое нажатие.
            event.repeat = state && state->down;
        }
        updateButton(state, event.type == EventType::KeyPressed);
        // Стороны модификатора независимы: отпускание левого Shift не снимает правый.
        if (event.key == Key::LeftShift || event.key == Key::RightShift) {
            event.modifiers.shift = key(Key::LeftShift).down || key(Key::RightShift).down;
        }
        if (event.key == Key::LeftControl || event.key == Key::RightControl) {
            event.modifiers.control = key(Key::LeftControl).down || key(Key::RightControl).down;
        }
        if (event.key == Key::LeftAlt || event.key == Key::RightAlt) {
            event.modifiers.alt = key(Key::LeftAlt).down || key(Key::RightAlt).down;
        }
        if (event.key == Key::LeftSuper || event.key == Key::RightSuper) {
            event.modifiers.super = key(Key::LeftSuper).down || key(Key::RightSuper).down;
        }
        modifierState = event.modifiers;
    }
    if (event.type == EventType::MouseMoved || event.type == EventType::MousePressed
        || event.type == EventType::MouseReleased || event.type == EventType::MouseWheel
        || event.type == EventType::MouseEntered) {
        if (event.type == EventType::MouseMoved && hasMousePosition) {
            event.deltaX = event.x - motionX;
            event.deltaY = event.y - motionY;
            mouseState.deltaX += event.deltaX;
            mouseState.deltaY += event.deltaY;
        }
        if (event.type == EventType::MouseMoved || !hasMousePosition) {
            motionX = event.x;
            motionY = event.y;
        }
        mouseState.x = event.x;
        mouseState.y = event.y;
        hasMousePosition = true;
        modifierState = event.modifiers;
    }
    if (event.type == EventType::MousePressed || event.type == EventType::MouseReleased) {
        updateButton(findState(buttons, event.button), event.type == EventType::MousePressed);
    }
    if (event.type == EventType::MouseWheel) {
        mouseState.wheelX += event.wheelX;
        mouseState.wheelY += event.wheelY;
    }
    if (event.type == EventType::TextInput) {
        frameText += event.text;
    }
    if (event.type == EventType::MouseEntered || event.type == EventType::MouseLeft) {
        mouseState.inside = event.type == EventType::MouseEntered;
    }
    if (event.type == EventType::FocusGained) {
        hasFocus = true;
        modifierState = event.modifiers;
    }
    const bool lostFocus = event.type == EventType::FocusLost
        || event.type == EventType::WindowMinimized || event.type == EventType::WindowDestroyed;
    frameEvents.push_back(event);
    if (lostFocus || event.type == EventType::MouseCaptureLost) {
        if (lostFocus) {
            hasFocus = false;
        }
        releaseButtons(event, lostFocus);
        if (lostFocus) {
            modifierState = {};
            hasMousePosition = false;
        }
        if (event.type == EventType::WindowDestroyed || event.type == EventType::WindowMinimized) {
            mouseState.inside = false;
        }
    }
}

ButtonState InputState::key(Key value) const
{
    const auto index = static_cast<std::size_t>(value);
    return index > 0 && index < keys.size() ? keys[index] : ButtonState{};
}

ButtonState InputState::button(MouseButton value) const
{
    const auto index = static_cast<std::size_t>(value);
    return index > 0 && index < buttons.size() ? buttons[index] : ButtonState{};
}

const MouseState& InputState::mouse() const
{
    return mouseState;
}

const Modifiers& InputState::modifiers() const
{
    return modifierState;
}

const std::vector<Event>& InputState::events() const
{
    return frameEvents;
}

const std::string& InputState::text() const
{
    return frameText;
}

bool InputState::focused() const
{
    return hasFocus;
}
}
