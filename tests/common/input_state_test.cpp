#include "input.h"

#include <iostream>
#include <stdexcept>

namespace
{
using namespace NeuexpEngine;

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

Event keyEvent(EventType type, Key key)
{
    Event event;
    event.type = type;
    event.key = key;
    return event;
}

void keyboard()
{
    InputState input;
    require(!input.focused() && input.events().empty() && input.text().empty(), "Initial input is not empty.");
    for (int index = 1; index < static_cast<int>(Key::Count); ++index) {
        const auto key = static_cast<Key>(index);
        input.beginFrame();
        input.handleEvent(keyEvent(EventType::KeyPressed, key));
        require(input.key(key).down && input.key(key).pressed && !input.key(key).released, "Key press was lost.");
        input.beginFrame();
        require(input.key(key).down && !input.key(key).pressed && input.events().empty(), "Held key did not survive a frame.");
        input.handleEvent(keyEvent(EventType::KeyPressed, key));
        require(input.events().back().repeat && !input.key(key).pressed, "Repeated key became a new press.");
        input.handleEvent(keyEvent(EventType::KeyReleased, key));
        require(!input.key(key).down && input.key(key).released, "Key release was lost.");
        input.beginFrame();
        require(!input.key(key).released, "Release did not expire.");
        input.handleEvent(keyEvent(EventType::KeyPressed, key));
        input.handleEvent(keyEvent(EventType::KeyReleased, key));
        input.handleEvent(keyEvent(EventType::KeyPressed, key));
        require(input.key(key).pressed && input.key(key).released && input.key(key).down,
            "Multiple transitions were collapsed into a snapshot.");
        require(input.events().size() == 3 && !input.events().back().repeat, "Transition order was lost.");
        input.handleEvent(keyEvent(EventType::KeyReleased, key));
    }
    input.beginFrame();
    for (const auto key : { Key::Unknown, Key::Count, static_cast<Key>(-1) }) {
        input.handleEvent(keyEvent(EventType::KeyPressed, key));
        require(!input.key(key).down, "Unknown key changed the snapshot.");
    }
    require(input.events().size() == 3, "Unknown key events must remain visible.");

    input = {};
    auto repeated = keyEvent(EventType::KeyPressed, Key::A);
    repeated.repeat = true;
    input.handleEvent(repeated);
    require(input.key(Key::A).down && input.key(Key::A).pressed && !input.events().back().repeat,
        "First observed key event must start a new press after cancellation.");
}

void modifiersAndFocus()
{
    InputState input;
    Event focus;
    focus.type = EventType::FocusGained;
    input.handleEvent(focus);
    require(input.focused(), "Focus was not gained.");
    for (const auto key : { Key::LeftShift, Key::RightShift, Key::LeftControl,
        Key::RightControl, Key::LeftAlt, Key::RightAlt, Key::LeftSuper, Key::RightSuper }) {
        input.handleEvent(keyEvent(EventType::KeyPressed, key));
    }
    input.handleEvent(keyEvent(EventType::KeyReleased, Key::LeftShift));
    require(input.modifiers().shift, "Right Shift was released with Left Shift.");
    input.handleEvent(keyEvent(EventType::KeyReleased, Key::RightShift));
    require(!input.modifiers().shift, "Shift stayed down after both releases.");
    input.handleEvent(keyEvent(EventType::KeyReleased, Key::LeftControl));
    require(input.modifiers().control, "Right Control was lost.");
    input.handleEvent(keyEvent(EventType::KeyReleased, Key::LeftAlt));
    require(input.modifiers().alt, "Right Alt was lost.");
    input.handleEvent(keyEvent(EventType::KeyReleased, Key::LeftSuper));
    require(input.modifiers().super, "Right Super was lost.");
    input.beginFrame();
    focus.type = EventType::FocusLost;
    focus.time = 42;
    input.handleEvent(focus);
    require(!input.focused() && input.events().size() == 4, "Focus cancellation lost held keys.");
    require(input.events().front().type == EventType::FocusLost, "Focus reason must precede cancellations.");
    for (std::size_t index = 1; index < input.events().size(); ++index) {
        const auto& event = input.events()[index];
        require(event.canceled && event.time == 42 && input.key(event.key).released,
            "Synthetic release lost its reason, time, or state.");
    }
    input.beginFrame();
    focus.type = EventType::FocusGained;
    input.handleEvent(focus);
    require(!input.key(Key::RightControl).down, "Focus regain resurrected a canceled key.");
}

void mouse()
{
    InputState input;
    for (int index = 1; index < static_cast<int>(MouseButton::Count); ++index) {
        Event event;
        event.type = EventType::MousePressed;
        event.button = static_cast<MouseButton>(index);
        event.x = -20;
        event.y = 800;
        input.handleEvent(event);
        require(input.button(event.button).down && input.button(event.button).pressed, "Mouse button was lost.");
        input.beginFrame();
        require(input.button(event.button).down && !input.button(event.button).pressed, "Mouse hold/reset failed.");
        event.type = EventType::MouseReleased;
        input.handleEvent(event);
        require(!input.button(event.button).down && input.button(event.button).released, "Mouse release was lost.");
    }
    input = {};
    Event event;
    event.type = EventType::MouseEntered;
    event.x = 10;
    event.y = 20;
    input.handleEvent(event);
    require(input.mouse().inside, "Mouse entry was lost.");
    event.type = EventType::MouseMoved;
    event.x = 30;
    event.y = 10;
    input.handleEvent(event);
    event.x = -5;
    event.y = 40;
    input.handleEvent(event);
    require(input.mouse().deltaX == -15 && input.mouse().deltaY == 20, "Motion did not accumulate.");
    require(input.events().back().deltaX == -35 && input.events().back().deltaY == 30, "Per-event motion was lost.");
    event.type = EventType::MouseWheel;
    event.wheelX = 0.5;
    event.wheelY = -0.25;
    input.handleEvent(event);
    input.handleEvent(event);
    require(input.mouse().wheelX == 1 && input.mouse().wheelY == -0.5, "Fractional wheel steps were lost.");
    input.beginFrame();
    require(input.mouse().x == -5 && input.mouse().deltaX == 0 && input.mouse().wheelY == 0,
        "Mouse frame reset cleared persistent position or retained deltas.");
    event = {};
    event.type = EventType::MouseLeft;
    input.handleEvent(event);
    require(!input.mouse().inside, "Mouse leave was lost.");
    for (const auto button : { MouseButton::Unknown, MouseButton::Count, static_cast<MouseButton>(-1) }) {
        event.type = EventType::MousePressed;
        event.button = button;
        input.handleEvent(event);
        require(!input.button(button).down, "Unknown mouse button changed the snapshot.");
    }
}

void cancellationAndText()
{
    for (const auto reason : { EventType::MouseCaptureLost, EventType::FocusLost,
        EventType::WindowMinimized, EventType::WindowDestroyed }) {
        InputState input;
        input.handleEvent(keyEvent(EventType::KeyPressed, Key::W));
        for (int index = 1; index < static_cast<int>(MouseButton::Count); ++index) {
            Event button;
            button.type = EventType::MousePressed;
            button.button = static_cast<MouseButton>(index);
            input.handleEvent(button);
        }
        input.beginFrame();
        Event event;
        event.type = reason;
        input.handleEvent(event);
        require(input.key(Key::W).down == (reason == EventType::MouseCaptureLost), "Cancellation affected the wrong devices.");
        for (int index = 1; index < static_cast<int>(MouseButton::Count); ++index) {
            const auto state = input.button(static_cast<MouseButton>(index));
            require(!state.down && state.released, "Cancellation left a mouse button held.");
        }
        const auto size = input.events().size();
        input.handleEvent(event);
        require(input.events().size() == size + 1, "Repeated cancellation duplicated releases.");
        if (reason != EventType::MouseCaptureLost) {
            input.beginFrame();
            event.type = EventType::MouseMoved;
            event.x = 200;
            event.y = 100;
            input.handleEvent(event);
            require(input.mouse().deltaX == 0 && input.mouse().deltaY == 0,
                "Canceled mouse releases restored a stale motion baseline.");
        }
    }
    InputState input;
    Event text;
    text.type = EventType::TextInput;
    text.text = u8"Ж😀";
    input.handleEvent(text);
    text.text = u8"e\u0301日本語";
    input.handleEvent(text);
    require(input.text() == u8"Ж😀e\u0301日本語" && input.events().size() == 2, "Unicode text was changed.");
    input.beginFrame();
    require(input.text().empty(), "Text survived frame reset.");
    for (const auto type : { EventType::WindowResized, EventType::WindowRestored, EventType::CloseRequested }) {
        Event window;
        window.type = type;
        window.width = 512;
        window.height = 300;
        window.time = 15;
        input.handleEvent(window);
        require(input.events().back().type == type && input.events().back().width == 512
            && input.events().back().time == 15, "Window event payload was lost.");
    }
}

void lockStates()
{
    InputState input;
    input.handleEvent(keyEvent(EventType::KeyPressed, Key::LeftShift));
    Event changed;
    changed.type = EventType::LockStateChanged;
    changed.modifiers.capsLock = true;
    changed.modifiers.numLock = true;
    input.handleEvent(changed);
    require(input.modifiers().capsLock && input.modifiers().numLock && input.modifiers().shift,
        "Lock notification must preserve held modifiers.");
    require(input.key(Key::LeftShift).down && !input.key(Key::CapsLock).down,
        "Lock notification fabricated physical key states.");
    input.beginFrame();
    require(input.modifiers().capsLock && input.modifiers().numLock, "Locks expired at frame boundary.");
    changed.modifiers = {};
    input.handleEvent(changed);
    require(!input.modifiers().capsLock && !input.modifiers().numLock && input.modifiers().shift,
        "Unlock notification changed unrelated modifiers.");
    require(input.events().size() == 1 && input.events().front().type == EventType::LockStateChanged,
        "Lock notification was not preserved.");
}
}

int main()
{
    try {
        keyboard();
        modifiersAndFocus();
        mouse();
        cancellationAndText();
        lockStates();
        std::cout << "Input state checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
