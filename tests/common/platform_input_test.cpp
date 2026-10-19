#include "platform.h"
#include "platform_test_diagnostics.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifndef _WIN32
// Подмена только результата метода ввода позволяет проверить Unicode/длинные commit
// без установленного сервера IME. Обычные нажатия проходят настоящий Xutf8LookupString.
namespace
{
std::string pendingCommit;
bool commitArmed = false;
bool filterNextKey = false;
bool legacyRepeat = false;
unsigned int closedDisplays = 0;
}
// Счётчик остаётся в тесте: проверяем закрытие соединения при отказе конструктора.
extern "C" int __real_XCloseDisplay(Display*);
extern "C" int __wrap_XCloseDisplay(Display* display)
{
    ++closedDisplays;
    return __real_XCloseDisplay(display);
}

extern "C" Bool __real_XkbSetDetectableAutoRepeat(Display*, Bool, Bool*);
extern "C" Bool __wrap_XkbSetDetectableAutoRepeat(Display* display, Bool detectable, Bool* supported)
{
    if (legacyRepeat) {
        *supported = False;
        return False;
    }
    return __real_XkbSetDetectableAutoRepeat(display, detectable, supported);
}

extern "C" Bool __real_XFilterEvent(XEvent*, Window);
extern "C" Bool __wrap_XFilterEvent(XEvent* event, Window window)
{
    if (filterNextKey && event->type == KeyPress) {
        filterNextKey = false;
        return True;
    }
    return __real_XFilterEvent(event, window);
}
extern "C" int __real_Xutf8LookupString(XIC, XKeyPressedEvent*, char*, int, KeySym*, Status*);
extern "C" int __wrap_Xutf8LookupString(XIC context, XKeyPressedEvent* event, char* buffer,
    int capacity, KeySym* symbol, Status* status)
{
    if (!commitArmed) {
        return __real_Xutf8LookupString(context, event, buffer, capacity, symbol, status);
    }
    const int length = static_cast<int>(pendingCommit.size());
    if (capacity < length) {
        *status = XBufferOverflow;
        return length;
    }
    std::memcpy(buffer, pendingCommit.data(), pendingCommit.size());
    *status = XLookupChars;
    commitArmed = false;
    return length;
}
#endif

namespace
{
using Engine::EventType;
using Engine::Key;
using Engine::MouseButton;

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void pump()
{
    Platform::Error error;
    require(PlatformTest::checkOperation(Platform::processEvents(error), error), "Event processing failed.");
}

const Engine::Event* findEvent(EventType type, Key key = Key::Unknown)
{
    for (const auto& event : Platform::getEvents()) {
        if (event.type == type && (key == Key::Unknown || event.key == key)) {
            return &event;
        }
    }
    return nullptr;
}

struct NativeInput
{
#ifdef _WIN32
    HWND window = nullptr;
    HKL layout = nullptr;
    bool ownsLayout = false;

    NativeInput()
    {
        window = FindWindowW(L"EngineWindow", L"Engine input test");
        require(window != nullptr, "Test window not found.");
        const int count = GetKeyboardLayoutList(0, nullptr);
        std::vector<HKL> layouts(static_cast<std::size_t>(count));
        GetKeyboardLayoutList(count, layouts.data());
        layout = LoadKeyboardLayoutW(L"00000409", KLF_NOTELLSHELL);
        require(layout != nullptr, "US keyboard layout is unavailable.");
        ownsLayout = std::find(layouts.begin(), layouts.end(), layout) == layouts.end();
    }

    ~NativeInput()
    {
        if (ownsLayout) {
            UnloadKeyboardLayout(layout);
        }
    }

    void send(UINT type, WPARAM value = 0, LPARAM data = 0)
    {
        require(PostMessageW(window, type, value, data) != FALSE, "PostMessage failed.");
    }

    void key(unsigned int code, bool down, bool repeat = false)
    {
        // MAPVK_VK_TO_VSC_EX возвращает scan code и префикс E0/E1 для выбранной раскладки.
        // LPARAM клавиатурного сообщения хранит число повторов в битах 0–15,
        // scan code в 16–23, признак E0 в 24, прежнее удержание в 30 и отпускание в 31.
        const UINT scan = MapVirtualKeyExW(code, MAPVK_VK_TO_VSC_EX, layout);
        LPARAM data = 1 | (static_cast<LPARAM>(scan & 0xffU) << 16);
        if ((scan & 0xff00U) == 0xe000U || (code >= VK_PRIOR && code <= VK_DOWN)
            || code == VK_INSERT || code == VK_DELETE || code == VK_DIVIDE) {
            data |= static_cast<LPARAM>(1) << 24;
        }
        if (!down || repeat) {
            data |= static_cast<LPARAM>(1) << 30;
        }
        if (!down) {
            data |= static_cast<LPARAM>(1) << 31;
        }
        send(down ? WM_KEYDOWN : WM_KEYUP, code, data);
    }

    void focus(bool focused)
    {
        // SendMessage проверяет также сообщения, пришедшие между processEvents.
        SendMessageW(window, focused ? WM_SETFOCUS : WM_KILLFOCUS, 0, 0);
    }

    void motion(int x, int y)
    {
        send(WM_MOUSEMOVE, 0, MAKELPARAM(static_cast<WORD>(x), static_cast<WORD>(y)));
    }

    void button(MouseButton button, bool down, int x = 23, int y = 42)
    {
        UINT message = 0;
        WPARAM data = 0;
        switch (button) {
            case MouseButton::Left: message = down ? WM_LBUTTONDOWN : WM_LBUTTONUP; break;
            case MouseButton::Right: message = down ? WM_RBUTTONDOWN : WM_RBUTTONUP; break;
            case MouseButton::Middle: message = down ? WM_MBUTTONDOWN : WM_MBUTTONUP; break;
            default:
                message = down ? WM_XBUTTONDOWN : WM_XBUTTONUP;
                data = MAKEWPARAM(0, button == MouseButton::Back ? XBUTTON1 : XBUTTON2);
                break;
        }
        send(message, data, MAKELPARAM(static_cast<WORD>(x), static_cast<WORD>(y)));
    }

    void wheel(bool horizontal, int amount)
    {
        POINT position = { 23, 42 };
        require(ClientToScreen(window, &position) != FALSE, "ClientToScreen failed.");
        send(horizontal ? WM_MOUSEHWHEEL : WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(amount * WHEEL_DELTA)),
            MAKELPARAM(static_cast<WORD>(position.x), static_cast<WORD>(position.y)));
    }

    void flush()
    {
    }
#else
    Display* display = nullptr;
    Window window = None;

    Window findWindow(Window root, const char* expectedTitle)
    {
        // XFetchName выделяет строку заголовка в Xlib; освобождение выполняет XFree.
        char* title = nullptr;
        XFetchName(display, root, &title);
        const bool found = title && std::strcmp(title, expectedTitle) == 0;
        if (title) { XFree(title); }
        if (found) { return root; }
        Window parent = None;
        Window actualRoot = None;
        Window* children = nullptr;
        unsigned int count = 0;
        Window result = None;
        if (XQueryTree(display, root, &actualRoot, &parent, &children, &count)) {
            for (unsigned int index = 0; index < count && result == None; ++index) {
                result = findWindow(children[index], expectedTitle);
            }
        }
        if (children) { XFree(children); }
        return result;
    }

    explicit NativeInput(const char* expectedTitle = "Engine input test")
    {
        // nullptr выбирает DISPLAY из окружения; каждый успешный XOpenDisplay
        // должен иметь парный XCloseDisplay, в том числе при ошибке конструктора.
        display = XOpenDisplay(nullptr);
        require(display != nullptr, "Could not open test display.");
        try {
            window = findWindow(DefaultRootWindow(display), expectedTitle);
            require(window != None, "Test window not found.");
        } catch (...) {
            // Деструктор NativeInput не вызывается, если его конструктор выбросил исключение.
            XCloseDisplay(display);
            display = nullptr;
            throw;
        }
    }

    ~NativeInput()
    {
        if (display) {
            XCloseDisplay(display);
        }
    }

    void send(XEvent& event, long mask)
    {
        // XSendEvent адресует сообщение конкретному окну с указанной маской подписки;
        // состояние настоящей клавиатуры и других приложений не изменяется.
        event.xany.display = display;
        event.xany.window = window;
        require(XSendEvent(display, window, False, mask, &event) != 0, "XSendEvent failed.");
    }

    void key(unsigned int symbol, bool down, bool = false)
    {
        XEvent event = {};
        event.xkey.type = down ? KeyPress : KeyRelease;
        event.xkey.keycode = XKeysymToKeycode(display, symbol);
        require(event.xkey.keycode != 0, "Requested key is absent in the test keyboard mapping.");
        event.xkey.root = DefaultRootWindow(display);
        event.xkey.same_screen = True;
        // Разные времена отличают короткое нажатие от старой пары autorepeat.
        static Time time = 1;
        event.xkey.time = time++;
        send(event, down ? KeyPressMask : KeyReleaseMask);
    }

    void focus(bool focused)
    {
        XEvent event = {};
        event.xfocus.type = focused ? FocusIn : FocusOut;
        event.xfocus.mode = NotifyNormal;
        event.xfocus.detail = NotifyNonlinear;
        send(event, FocusChangeMask);
    }

    void motion(int x, int y)
    {
        XEvent event = {};
        event.xmotion.type = MotionNotify;
        event.xmotion.x = x;
        event.xmotion.y = y;
        event.xmotion.same_screen = True;
        send(event, PointerMotionMask);
    }

    void nativeButton(unsigned int code, bool down, int x, int y)
    {
        XEvent event = {};
        event.xbutton.type = down ? ButtonPress : ButtonRelease;
        event.xbutton.button = code;
        event.xbutton.x = x;
        event.xbutton.y = y;
        event.xbutton.same_screen = True;
        send(event, down ? ButtonPressMask : ButtonReleaseMask);
    }

    void button(MouseButton button, bool down, int x = 23, int y = 42)
    {
        unsigned int code = 0;
        switch (button) {
            case MouseButton::Left: code = 1; break;
            case MouseButton::Middle: code = 2; break;
            case MouseButton::Right: code = 3; break;
            case MouseButton::Back: code = 8; break;
            case MouseButton::Forward: code = 9; break;
            default: break;
        }
        nativeButton(code, down, x, y);
    }

    void wheel(bool horizontal, int amount)
    {
        const unsigned int code = horizontal ? (amount > 0 ? 7U : 6U) : (amount > 0 ? 4U : 5U);
        for (int index = 0; index < std::abs(amount); ++index) {
            nativeButton(code, true, 23, 42);
            nativeButton(code, false, 23, 42);
        }
    }

    void flush()
    {
        // False сохраняет очередь: XSync ждёт обработки запросов сервером, не удаляя события.
        XSync(display, False);
    }
#endif

    void frame()
    {
        flush();
        pump();
    }
};

void keyboard(NativeInput& native)
{
    native.focus(true);
    native.frame();
    require(Platform::getInput().focused(), "Input did not receive focus.");
    std::vector<std::pair<Key, unsigned int>> keys;
    for (int index = 0; index < 26; ++index) {
#ifdef _WIN32
        const unsigned int code = static_cast<unsigned int>('A' + index);
#else
        const unsigned int code = static_cast<unsigned int>('a' + index);
#endif
        keys.emplace_back(static_cast<Key>(static_cast<int>(Key::A) + index), code);
    }
    for (int index = 0; index < 10; ++index) {
        keys.emplace_back(static_cast<Key>(static_cast<int>(Key::Digit0) + index), static_cast<unsigned int>('0' + index));
    }
#ifdef _WIN32
    const std::pair<Key, unsigned int> special[] = {
        {Key::Escape, VK_ESCAPE}, {Key::Enter, VK_RETURN}, {Key::Tab, VK_TAB}, {Key::Backspace, VK_BACK},
        {Key::Space, VK_SPACE}, {Key::LeftShift, VK_LSHIFT}, {Key::RightShift, VK_RSHIFT},
        {Key::LeftControl, VK_LCONTROL}, {Key::RightControl, VK_RCONTROL},
        {Key::LeftAlt, VK_LMENU}, {Key::RightAlt, VK_RMENU}, {Key::LeftSuper, VK_LWIN}, {Key::RightSuper, VK_RWIN},
        {Key::Menu, VK_APPS}, {Key::CapsLock, VK_CAPITAL}, {Key::NumLock, VK_NUMLOCK},
        {Key::ScrollLock, VK_SCROLL}, {Key::PrintScreen, VK_SNAPSHOT}, {Key::Pause, VK_PAUSE},
        {Key::Insert, VK_INSERT}, {Key::Delete, VK_DELETE}, {Key::Home, VK_HOME}, {Key::End, VK_END},
        {Key::PageUp, VK_PRIOR}, {Key::PageDown, VK_NEXT}, {Key::ArrowUp, VK_UP}, {Key::ArrowDown, VK_DOWN},
        {Key::ArrowLeft, VK_LEFT}, {Key::ArrowRight, VK_RIGHT},
        {Key::Minus, VK_OEM_MINUS}, {Key::Equal, VK_OEM_PLUS}, {Key::LeftBracket, VK_OEM_4},
        {Key::RightBracket, VK_OEM_6}, {Key::Backslash, VK_OEM_5}, {Key::Semicolon, VK_OEM_1},
        {Key::Apostrophe, VK_OEM_7}, {Key::Grave, VK_OEM_3}, {Key::Comma, VK_OEM_COMMA},
        {Key::Period, VK_OEM_PERIOD}, {Key::Slash, VK_OEM_2}, {Key::InternationalBackslash, VK_OEM_102},
        {Key::NumpadDecimal, VK_DECIMAL}, {Key::NumpadDivide, VK_DIVIDE},
        {Key::NumpadMultiply, VK_MULTIPLY}, {Key::NumpadSubtract, VK_SUBTRACT}, {Key::NumpadAdd, VK_ADD}
    };
    constexpr unsigned int FunctionBase = VK_F1;
    constexpr unsigned int NumpadBase = VK_NUMPAD0;
    constexpr int FunctionCount = 24;
#else
    const std::pair<Key, unsigned int> special[] = {
        {Key::Escape, XK_Escape}, {Key::Enter, XK_Return}, {Key::Tab, XK_Tab}, {Key::Backspace, XK_BackSpace},
        {Key::Space, XK_space}, {Key::LeftShift, XK_Shift_L}, {Key::RightShift, XK_Shift_R},
        {Key::LeftControl, XK_Control_L}, {Key::RightControl, XK_Control_R},
        {Key::LeftAlt, XK_Alt_L}, {Key::RightAlt, XK_Alt_R}, {Key::LeftSuper, XK_Super_L}, {Key::RightSuper, XK_Super_R},
        {Key::Menu, XK_Menu}, {Key::CapsLock, XK_Caps_Lock}, {Key::NumLock, XK_Num_Lock},
        {Key::ScrollLock, XK_Scroll_Lock}, {Key::PrintScreen, XK_Print}, {Key::Pause, XK_Pause},
        {Key::Insert, XK_Insert}, {Key::Delete, XK_Delete}, {Key::Home, XK_Home}, {Key::End, XK_End},
        {Key::PageUp, XK_Page_Up}, {Key::PageDown, XK_Page_Down}, {Key::ArrowUp, XK_Up}, {Key::ArrowDown, XK_Down},
        {Key::ArrowLeft, XK_Left}, {Key::ArrowRight, XK_Right},
        {Key::Minus, XK_minus}, {Key::Equal, XK_equal}, {Key::LeftBracket, XK_bracketleft},
        {Key::RightBracket, XK_bracketright}, {Key::Backslash, XK_backslash}, {Key::Semicolon, XK_semicolon},
        {Key::Apostrophe, XK_apostrophe}, {Key::Grave, XK_grave}, {Key::Comma, XK_comma},
        {Key::Period, XK_period}, {Key::Slash, XK_slash},
        {Key::NumpadEnter, XK_KP_Enter}, {Key::NumpadDecimal, XK_KP_Delete}, {Key::NumpadDivide, XK_KP_Divide},
        {Key::NumpadMultiply, XK_KP_Multiply}, {Key::NumpadSubtract, XK_KP_Subtract}, {Key::NumpadAdd, XK_KP_Add}
    };
    constexpr unsigned int FunctionBase = XK_F1;
    constexpr unsigned int NumpadBase = XK_KP_0;
    // F13-F24 отсутствуют на стандартной X11 pc105; их состояние покрывает общий тест.
    constexpr int FunctionCount = 12;
#endif
    keys.insert(keys.end(), std::begin(special), std::end(special));
    for (int index = 0; index < FunctionCount; ++index) {
        keys.emplace_back(static_cast<Key>(static_cast<int>(Key::F1) + index), FunctionBase + static_cast<unsigned int>(index));
    }
    for (int index = 0; index < 10; ++index) {
        keys.emplace_back(static_cast<Key>(static_cast<int>(Key::Numpad0) + index), NumpadBase + static_cast<unsigned int>(index));
    }
    for (const auto& item : keys) {
        native.key(item.second, true);
        native.frame();
        if (!Platform::isKeyPressed(item.first) || !Platform::isKeyDown(item.first)) {
            std::cerr << "Key mapping failed: expected=" << static_cast<int>(item.first) << " native=" << item.second << '\n';
            throw std::runtime_error("Physical key mapping failed.");
        }
        native.key(item.second, false);
        native.frame();
        require(Platform::isKeyReleased(item.first) && !Platform::isKeyDown(item.first), "Native key release was lost.");
    }
    const unsigned int a = keys.front().second;
    native.key(a, true);
    native.key(a, false);
    native.frame();
    require(Platform::isKeyPressed(Key::A) && Platform::isKeyReleased(Key::A) && !Platform::isKeyDown(Key::A),
        "A short native press disappeared between frames.");
    const auto* press = findEvent(EventType::KeyPressed, Key::A);
    const auto* release = findEvent(EventType::KeyReleased, Key::A);
    require(press && release && press < release && press->time <= release->time, "Native transition order was lost.");
    native.key(a, true);
    native.frame();
    native.key(a, true, true);
    native.frame();
    const auto* repeated = findEvent(EventType::KeyPressed, Key::A);
    require(!Platform::isKeyPressed(Key::A) && repeated && repeated->repeat, "Native autorepeat became a press.");
    require(!Platform::isCloseRequested(), "Keyboard input requested application close.");
    native.focus(false);
    native.frame();
    const auto* canceled = findEvent(EventType::KeyReleased, Key::A);
    require(!Platform::isKeyDown(Key::A) && Platform::isKeyReleased(Key::A)
        && canceled && canceled->canceled, "Focus loss did not cancel keyboard input.");
    native.key(a, true);
    native.frame();
    require(!Platform::isKeyDown(Key::A), "Unfocused window accepted a key.");
    native.focus(true);
    native.frame();
    require(!Platform::isKeyDown(Key::A), "Focus regain resurrected a key.");
#ifdef _WIN32
    // Цифровой Enter имеет общий VK_RETURN, но отдельный E0 scan code.
    native.send(WM_KEYDOWN, VK_RETURN, 1 | (static_cast<LPARAM>(0x11c) << 16));
    native.send(WM_KEYUP, VK_RETURN, 1 | (static_cast<LPARAM>(0x11c) << 16) | (static_cast<LPARAM>(3) << 30));
    native.frame();
    require(Platform::isKeyPressed(Key::NumpadEnter) && Platform::isKeyReleased(Key::NumpadEnter), "Numpad Enter was merged with Enter.");
    native.send(WM_KEYDOWN, VK_NUMLOCK, 1 | (static_cast<LPARAM>(0x145) << 16));
    native.send(WM_KEYUP, VK_NUMLOCK, 1 | (static_cast<LPARAM>(0x145) << 16) | (static_cast<LPARAM>(3) << 30));
    native.frame();
    require(Platform::isKeyPressed(Key::NumLock) && !Platform::isKeyPressed(Key::Pause), "Extended NumLock became Pause.");
    native.key(VK_LSHIFT, true);
    native.key(VK_RSHIFT, true);
    native.key(VK_LSHIFT, false);
    native.frame();
    require(Platform::getInput().modifiers().shift, "Native left Shift release cleared right Shift.");
    native.key(VK_RSHIFT, false);
    native.frame();
    require(!Platform::getInput().modifiers().shift, "Native Shift stayed down.");
#else
    if (legacyRepeat) {
        native.key(XK_a, true);
        native.frame();
        XEvent repeatPair = {};
        repeatPair.xkey.type = KeyRelease;
        repeatPair.xkey.keycode = XKeysymToKeycode(native.display, XK_a);
        repeatPair.xkey.time = 500;
        native.send(repeatPair, KeyReleaseMask);
        repeatPair.xkey.type = KeyPress;
        native.send(repeatPair, KeyPressMask);
        native.frame();
        const auto* legacyEvent = findEvent(EventType::KeyPressed, Key::A);
        require(!Platform::isKeyReleased(Key::A) && !Platform::isKeyPressed(Key::A)
            && Platform::isKeyDown(Key::A) && legacyEvent && legacyEvent->repeat,
            "Legacy X11 autorepeat became release/press.");
        native.key(XK_a, false);
        native.frame();
    }
#endif
}

void mouse(NativeInput& native)
{
    native.motion(10, 20);
    native.frame();
    native.motion(30, 40);
    native.motion(25, 50);
    native.frame();
    const auto& mouse = Platform::getInput().mouse();
    if (mouse.x != 25 || mouse.y != 50 || mouse.deltaX != 15 || mouse.deltaY != 30) {
        std::cerr << "Mouse: " << mouse.x << ',' << mouse.y << " delta=" << mouse.deltaX << ',' << mouse.deltaY << '\n';
        for (const auto& event : Platform::getEvents()) {
            std::cerr << "Event " << static_cast<int>(event.type) << " xy=" << event.x << ',' << event.y
                << " delta=" << event.deltaX << ',' << event.deltaY << '\n';
        }
    }
    require(mouse.x == 25 && mouse.y == 50 && mouse.deltaX == 15 && mouse.deltaY == 30, "Native mouse movement was lost.");
    for (const auto button : { MouseButton::Left, MouseButton::Right, MouseButton::Middle, MouseButton::Back, MouseButton::Forward }) {
        native.button(button, true);
        native.button(button, false, -15, 500);
        native.frame();
        const auto state = Platform::getInput().button(button);
        require(state.pressed && state.released && !state.down, "Native mouse button transitions were lost.");
        require(Platform::getInput().mouse().x == -15, "Outside mouse coordinates were clamped or unsigned.");
    }
    native.wheel(false, 2);
    native.wheel(false, -1);
    native.wheel(true, 1);
    native.frame();
    require(mouse.wheelX == 1 && mouse.wheelY == 1 && mouse.x == 23 && mouse.y == 42, "Wheel axes/units/coordinates differ.");
    native.frame();
    require(mouse.wheelX == 0 && mouse.wheelY == 0 && mouse.deltaX == 0, "Mouse deltas survived a frame.");
    native.button(MouseButton::Left, true);
    native.frame();
    native.focus(false);
    native.frame();
    require(!Platform::getInput().button(MouseButton::Left).down
        && Platform::getInput().button(MouseButton::Left).released, "Focus loss left a mouse button held.");
    native.focus(true);
    native.frame();
#ifdef _WIN32
    native.button(MouseButton::Left, true);
    native.frame();
    require(GetCapture() == native.window, "Drag did not capture the pointer.");
    ReleaseCapture();
    native.frame();
    require(findEvent(EventType::MouseCaptureLost) && !Platform::getInput().button(MouseButton::Left).down,
        "Capture loss did not cancel drag.");
    native.send(WM_MOUSEWHEEL, MAKEWPARAM(0, 30), 0);
    native.frame();
    require(mouse.wheelY == 0.25, "High resolution wheel lost fractional steps.");
    native.motion(15, 20);
    native.frame();
    // Реальный курсор не перемещается тестом: Windows вправе сразу прислать MouseLeave.
    require(findEvent(EventType::MouseEntered), "Mouse entry was lost.");
    native.motion(16, 20);
    native.send(WM_MOUSELEAVE);
#else
    XEvent crossing = {};
    crossing.xcrossing.type = EnterNotify;
    crossing.xcrossing.mode = NotifyNormal;
    native.send(crossing, EnterWindowMask);
    native.frame();
    require(mouse.inside && findEvent(EventType::MouseEntered), "Mouse entry was lost.");
    crossing.xcrossing.type = LeaveNotify;
    native.send(crossing, LeaveWindowMask);
#endif
    native.frame();
    require(!mouse.inside && findEvent(EventType::MouseLeft), "Mouse leave was lost.");
}

void text(NativeInput& native)
{
#ifdef _WIN32
    native.send(WM_CHAR, 0x416, 1);
    native.send(WM_CHAR, 0xd83d, 1);
    native.send(WM_CHAR, 0xde00, 1);
    native.send(WM_UNICHAR, 0x1f642);
    native.frame();
    if (Platform::getInput().text() != u8"Ж😀🙂") {
        std::cerr << "Text bytes (focus=" << Platform::getInput().focused() << "):";
        for (char byte : Platform::getInput().text()) { std::cerr << ' ' << static_cast<unsigned int>(static_cast<unsigned char>(byte)); }
        std::cerr << '\n';
    }
    require(Platform::getInput().text() == u8"Ж😀🙂", "UTF-16/UTF-32 conversion failed.");
    require(SendMessageW(native.window, WM_UNICHAR, UNICODE_NOCHAR, 0) == TRUE, "WM_UNICHAR support query failed.");
    native.send(WM_CHAR, 0xd800, 1);
    native.send(WM_CHAR, 'a', 1);
    native.send(WM_CHAR, 0xdc00, 1);
    native.frame();
    require(Platform::getInput().text() == u8"\ufffda\ufffd", "Invalid UTF-16 was not replaced.");
    native.send(WM_CHAR, 0x65e5, 1);
    native.send(WM_UNICHAR, 0x110000);
    native.send(WM_CHAR, 0xd800, 1);
    native.send(WM_UNICHAR, 'z');
    native.frame();
    require(Platform::getInput().text() == u8"日\ufffd\ufffdz", "Invalid scalar/mixed text messages were not handled.");
    native.send(WM_CHAR, 'x', 3);
    native.send(WM_CHAR, '\r', 1);
    native.send(WM_CHAR, '\t', 1);
    native.frame();
    require(Platform::getInput().text() == "xxx", "Character repeat/control handling failed.");
    native.send(WM_CHAR, 0xd800, 1);
    native.frame();
    native.focus(false);
    native.frame();
    native.focus(true);
    native.send(WM_CHAR, 'a', 1);
    native.frame();
    require(Platform::getInput().text() == "a", "Focus loss retained an incomplete surrogate.");
#else
    filterNextKey = true;
    native.key(XK_a, true);
    native.frame();
    require(!filterNextKey && Platform::isKeyPressed(Key::A) && Platform::getInput().text().empty(),
        "XIM filtering lost the physical transition or leaked uncommitted text.");
    native.key(XK_a, false);
    native.frame();
    pendingCommit = u8"Ж😀🙂日本語" + std::string(300, 'x');
    commitArmed = true;
    native.key(XK_a, true);
    native.key(XK_a, false);
    native.frame();
    require(Platform::getInput().text() == pendingCommit && !commitArmed, "XIM Unicode/overflow commit was lost.");
    native.key(XK_a, true);
    native.key(XK_a, false);
    native.frame();
    require(Platform::getInput().text() == "a", "Native XIM lookup did not produce text.");
    native.key(XK_Return, true);
    native.key(XK_Return, false);
    native.frame();
    require(Platform::getInput().text().empty(), "Control characters became text.");
#endif
    native.frame();
    require(Platform::getInput().text().empty(), "Text did not expire.");
}

void windowEvents(NativeInput& native)
{
#ifdef _WIN32
    // Синхронные сообщения не должны пропасть при начале следующего кадра.
    SendMessageW(native.window, WM_SIZE, SIZE_RESTORED, MAKELPARAM(450, 320));
#else
    XResizeWindow(native.display, native.window, 450, 320);
#endif
    native.frame();
    const auto* resized = findEvent(EventType::WindowResized);
    require(resized && resized->width == 450 && resized->height == 320, "Resize event payload was lost.");
#ifdef _WIN32
    native.send(WM_SIZE, SIZE_MINIMIZED, 0);
#else
    const Atom state = XInternAtom(native.display, "WM_STATE", False);
    const long minimized[] = { IconicState, None };
    XChangeProperty(native.display, native.window, state, state, 32, PropModeReplace,
        reinterpret_cast<const unsigned char*>(minimized), 2);
#endif
    native.frame();
    require(findEvent(EventType::WindowMinimized), "Minimize event was lost.");
#ifdef _WIN32
    native.send(WM_SIZE, SIZE_RESTORED, MAKELPARAM(450, 320));
#else
    const long restored[] = { NormalState, None };
    XChangeProperty(native.display, native.window, state, state, 32, PropModeReplace,
        reinterpret_cast<const unsigned char*>(restored), 2);
#endif
    native.frame();
    require(findEvent(EventType::WindowRestored), "Restore event was lost.");
#ifdef _WIN32
    native.send(WM_CLOSE);
#else
    XEvent close = {};
    close.xclient.type = ClientMessage;
    close.xclient.message_type = XInternAtom(native.display, "WM_PROTOCOLS", False);
    close.xclient.format = 32;
    close.xclient.data.l[0] = static_cast<long>(XInternAtom(native.display, "WM_DELETE_WINDOW", False));
    native.send(close, NoEventMask);
#endif
    native.frame();
    require(findEvent(EventType::CloseRequested) && Platform::isCloseRequested(), "Close request event was lost.");
    require(!findEvent(EventType::WindowDestroyed), "Close request destroyed the window automatically.");
#ifdef _WIN32
    require(DestroyWindow(native.window) != FALSE, "External window destruction failed.");
#else
    XDestroyWindow(native.display, native.window);
#endif
    native.frame();
    require(findEvent(EventType::WindowDestroyed) && !Platform::getInput().focused(), "Window destruction event was lost.");
    native.frame();
    require(Platform::getEvents().empty(), "Destroyed window retained old input events.");
}
}

int main(int argc, char** argv)
{
#ifdef _WIN32
    (void)argc;
    (void)argv;
#else
    legacyRepeat = argc > 1 && std::string(argv[1]) == "legacy-repeat";
#endif
#ifndef _WIN32
    if (!std::getenv("DISPLAY")) {
        std::cout << "Native input tests require an X11 display.\n";
        return 77;
    }
#endif
    std::vector<Platform::Error> errors;
    try {
#ifndef _WIN32
        if (argc > 1 && std::string(argv[1]) == "constructor-rollback") {
            const unsigned int closedBefore = closedDisplays;
            bool failedAsExpected = false;
            try {
                // Окно с этим заголовком не создаётся: ошибка возникает после XOpenDisplay.
                NativeInput native("Engine input missing window for constructor rollback");
            } catch (const std::runtime_error& error) {
                failedAsExpected = std::string(error.what()) == "Test window not found.";
            }
            require(failedAsExpected, "The missing window did not trigger constructor failure.");
            require(closedDisplays == closedBefore + 1, "Constructor failure leaked the X11 connection.");
            std::cout << "Native input constructor rollback passed.\n";
            return 0;
        }
#endif
        require(Platform::getEvents().empty() && !Platform::isKeyDown(Key::A), "Input exists before initialization.");
        Platform::WindowOptions options;
        options.title = "Engine input test";
        options.width = 400;
        options.height = 300;
        if (!Platform::initialize(options, errors)) {
            PlatformTest::reportErrors(errors);
            throw std::runtime_error("Initialization failed.");
        }
        {
            NativeInput native;
            keyboard(native);
            mouse(native);
            text(native);
            windowEvents(native);
        }
        Platform::shutdown(errors);
        require(errors.empty(), "Shutdown failed.");
        require(Platform::getEvents().empty() && Platform::getInput().text().empty()
            && !Platform::getInput().focused() && !Platform::isKeyDown(Key::A), "Shutdown retained input.");
        require(Platform::initialize(options, errors), "Reinitialization failed.");
        require(!Platform::isKeyDown(Key::A), "Reinitialization retained held keys.");
        Platform::shutdown(errors);
        require(errors.empty(), "Second shutdown failed.");
        std::cout << "Native input checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        Platform::shutdown(errors);
        PlatformTest::reportErrors(errors);
        return 1;
    }
}
