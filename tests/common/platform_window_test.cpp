#include "platform.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <unistd.h>
#include <dlfcn.h>
#include "../linux/keyboard_query_counter.h"
#endif

#include <GL/gl.h>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

Platform::WindowState readState()
{
    Platform::Error error;
    const auto result = Platform::processEvents(error);
    require(result, error.operation + ": " + error.message);
    Platform::WindowState state;
    const bool success = Platform::getWindowState(state, error);
    require(success, error.operation + ": " + error.message);
    return state;
}

template<class Predicate>
void waitFor(Predicate predicate, const char* message)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    do {
        if (predicate(readState())) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < deadline);
    throw std::runtime_error(message);
}

// Нативные вызовы допустимы здесь: тест проверяет общий API через реальные события ОС.
struct NativeWindow
{
#ifdef _WIN32
    HWND handle = nullptr;

    std::wstring toWide(const std::string& text)
    {
        const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), -1, nullptr, 0);
        require(size > 0, "Could not convert expected title.");
        std::wstring result(static_cast<std::size_t>(size), L'\0');
        require(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), -1,
            result.data(), size) != 0, "Could not convert expected title.");
        result.resize(static_cast<std::size_t>(size - 1));
        return result;
    }

    void checkTitle(const std::string& title)
    {
        std::wstring actual(static_cast<std::size_t>(GetWindowTextLengthW(handle) + 1), L'\0');
        const int size = GetWindowTextW(handle, actual.data(), static_cast<int>(actual.size()));
        actual.resize(static_cast<std::size_t>(size));
        require(actual == toWide(title), "The native Unicode title differs from the requested title.");
    }

    explicit NativeWindow(const std::string& title)
    {
        const std::wstring wideTitle = toWide(title);
        handle = FindWindowW(L"EngineWindow", wideTitle.c_str());
        require(handle != nullptr, "Could not find the test window.");
    }

    void checkMinimum(const Platform::WindowOptions& options)
    {
        MINMAXINFO limits = {};
        SendMessage(handle, WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&limits));
        if (!options.resizable) {
            const auto style = static_cast<DWORD>(GetWindowLongPtr(handle, GWL_STYLE));
            require((style & (WS_THICKFRAME | WS_MAXIMIZEBOX)) == 0,
                "A fixed window must not have resizing or maximize controls.");
            RECT fixedBounds = { 0, 0, options.width, options.height };
            require(AdjustWindowRectEx(&fixedBounds, style, FALSE, 0) != FALSE,
                "Could not calculate the fixed frame size.");
            require(limits.ptMinTrackSize.x == fixedBounds.right - fixedBounds.left
                && limits.ptMinTrackSize.y == fixedBounds.bottom - fixedBounds.top
                && limits.ptMinTrackSize.x == limits.ptMaxTrackSize.x
                && limits.ptMinTrackSize.y == limits.ptMaxTrackSize.y,
                "A fixed window must have identical minimum and maximum track dimensions.");
            return;
        }
        RECT bounds = { 0, 0, options.minimumWidth, options.minimumHeight };
        require(AdjustWindowRectEx(&bounds, WS_OVERLAPPEDWINDOW, FALSE, 0) != FALSE,
            "Could not calculate expected frame size.");
        require(options.minimumWidth == 0 || limits.ptMinTrackSize.x == bounds.right - bounds.left,
            "Minimum client width was not converted to the frame width.");
        require(options.minimumHeight == 0 || limits.ptMinTrackSize.y == bounds.bottom - bounds.top,
            "Minimum client height was not converted to the frame height.");
    }

    void resize()
    {
        RECT bounds = { 0, 0, 400, 300 };
        require(AdjustWindowRectEx(&bounds, WS_OVERLAPPEDWINDOW, FALSE, 0) != FALSE,
            "Could not calculate resized frame.");
        require(SetWindowPos(handle, nullptr, 0, 0, bounds.right - bounds.left,
            bounds.bottom - bounds.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
            "Could not resize the window.");
    }

    void focus(bool focused)
    {
        SetFocus(focused ? handle : nullptr);
    }

    bool minimize()
    {
        ShowWindow(handle, SW_MINIMIZE);
        return true;
    }

    void restore()
    {
        ShowWindow(handle, SW_RESTORE);
    }

    void close()
    {
        require(PostMessage(handle, WM_CLOSE, 0, 0) != FALSE, "Could not request close.");
    }

    void destroy()
    {
        require(DestroyWindow(handle) != FALSE, "Could not destroy the native window.");
    }
#else
    Display* display = nullptr;
    Window handle = None;
    Window focusSink = None;
    void* inputLibrary = nullptr;

    std::string readTitle(Window target)
    {
        const Atom name = XInternAtom(display, "_NET_WM_NAME", False);
        const Atom utf8 = XInternAtom(display, "UTF8_STRING", False);
        Atom type = None;
        int format = 0;
        unsigned long count = 0;
        unsigned long remaining = 0;
        unsigned char* bytes = nullptr;
        XGetWindowProperty(display, target, name, 0, 4096, False, utf8,
            &type, &format, &count, &remaining, &bytes);
        std::string result;
        if (bytes && type == utf8 && format == 8) {
            result.assign(reinterpret_cast<const char*>(bytes), count);
        }
        if (bytes) {
            XFree(bytes);
        }
        return result;
    }

    void checkTitle(const std::string& title)
    {
        require(readTitle(handle) == title, "The native UTF-8 title differs from the requested title.");
    }

    Window findWindow(Window parent, const std::string& title)
    {
        const bool found = readTitle(parent) == title;
        if (found) {
            return parent;
        }
        Window root = None;
        Window actualParent = None;
        Window* children = nullptr;
        unsigned int count = 0;
        XQueryTree(display, parent, &root, &actualParent, &children, &count);
        Window result = None;
        for (unsigned int index = 0; index < count && result == None; ++index) {
            result = findWindow(children[index], title);
        }
        if (children) {
            XFree(children);
        }
        return result;
    }

    explicit NativeWindow(const std::string& title)
    {
        display = XOpenDisplay(nullptr);
        require(display != nullptr, "Could not open the test X11 connection.");
        handle = findWindow(DefaultRootWindow(display), title);
        if (handle == None) {
            XCloseDisplay(display);
            display = nullptr;
            throw std::runtime_error("Could not find the test window.");
        }
    }

    ~NativeWindow()
    {
        if (display) {
            if (focusSink != None) {
                XDestroyWindow(display, focusSink);
            }
            XCloseDisplay(display);
        }
        // XTest регистрирует обработчик XCloseDisplay, поэтому выгружается после соединения.
        if (inputLibrary) {
            dlclose(inputLibrary);
        }
    }

    void checkMinimum(const Platform::WindowOptions& options)
    {
        XSizeHints hints = {};
        long supplied = 0;
        require(XGetWMNormalHints(display, handle, &hints, &supplied) != 0,
            "Could not read WM_NORMAL_HINTS.");
        if (!options.resizable) {
            require((hints.flags & (PMinSize | PMaxSize)) == (PMinSize | PMaxSize)
                && hints.min_width == options.width && hints.max_width == options.width
                && hints.min_height == options.height && hints.max_height == options.height,
                "A fixed window must have identical minimum and maximum client dimensions.");
            return;
        }
        require((hints.flags & PMinSize) != 0 && hints.min_width == options.minimumWidth
            && hints.min_height == options.minimumHeight, "Incorrect minimum client dimensions.");
    }

    void resize()
    {
        XResizeWindow(display, handle, 400, 300);
        XSync(display, False);
    }

    void focus(bool focused)
    {
        // PointerRoot позволяет менеджеру вернуть фокус окну под курсором.
        // Отдельное неменеджеруемое окно делает передачу фокуса однозначной.
        if (!focused && focusSink == None) {
            XSetWindowAttributes attributes = {};
            attributes.override_redirect = True;
            focusSink = XCreateWindow(display, DefaultRootWindow(display), 0, 0, 1, 1,
                0, 0, InputOnly, CopyFromParent, CWOverrideRedirect, &attributes);
            XMapWindow(display, focusSink);
        }
        XSetInputFocus(display, focused ? handle : focusSink, RevertToPointerRoot, CurrentTime);
        XSync(display, False);
    }

    bool minimize()
    {
        // Без оконного менеджера X-сервер сам не выполняет запрос сворачивания.
        const Atom stateAtom = XInternAtom(display, "WM_STATE", True);
        Atom type = None;
        int format = 0;
        unsigned long count = 0;
        unsigned long remaining = 0;
        unsigned char* value = nullptr;
        if (stateAtom == None) {
            return false;
        }
        XGetWindowProperty(display, handle, stateAtom, 0, 2, False, stateAtom,
            &type, &format, &count, &remaining, &value);
        const bool managed = type == stateAtom && count > 0;
        if (value) {
            XFree(value);
        }
        if (!managed) {
            return false;
        }
        require(XIconifyWindow(display, handle, DefaultScreen(display)) != 0,
            "Could not request minimization.");
        XFlush(display);
        return true;
    }

    void restore()
    {
        XMapWindow(display, handle);
        XFlush(display);
    }

    void close()
    {
        XEvent event = {};
        event.xclient.type = ClientMessage;
        event.xclient.window = handle;
        event.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", False);
        event.xclient.format = 32;
        event.xclient.data.l[0] = static_cast<long>(XInternAtom(display, "WM_DELETE_WINDOW", False));
        require(XSendEvent(display, handle, False, NoEventMask, &event) != 0,
            "Could not request close.");
        XSync(display, False);
    }

    void destroy()
    {
        XDestroyWindow(display, handle);
        XSync(display, False);
    }
#endif
};

void shutdown()
{
    std::vector<Platform::Error> errors;
    Platform::shutdown(errors);
    require(errors.empty(), errors.empty() ? "" : errors.front().operation + ": " + errors.front().message);
    Platform::WindowState state;
    Platform::Error error;
    require(!Platform::getWindowState(state, error), "State query after shutdown must fail.");
    require(!Platform::isCloseRequested(), "Shutdown must reset the close request.");
}

// Проверяем реальный задний буфер без зависимости от сцены и игрового рендера.
void checkRendering()
{
    const auto state = readState();
    glViewport(0, 0, state.width, state.height);
    glDrawBuffer(GL_BACK);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    // Два цвета исключают успех за счёт содержимого предыдущего кадра.
    for (const auto& color : { std::array<GLubyte, 3>{ 255, 0, 0 },
                              std::array<GLubyte, 3>{ 0, 255, 0 } }) {
        glClearColor(color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        std::array<GLubyte, 3> pixel = {};
        // glReadPixels ждёт завершения команд рисования до чтения результата.
        glReadPixels(state.width / 2, state.height / 2, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel.data());
        require(glGetError() == GL_NO_ERROR && pixel == color,
            "The OpenGL back buffer does not contain the rendered color.");
        Platform::Error error;
        require(Platform::present(error), "Present failed after rendering.");
    }
}

// Ввод освобождается и при провале проверки, чтобы Escape не остался нажатым.
class EscapeInput
{
public:
#ifndef _WIN32
    using FakeKeyEvent = int (*)(Display*, unsigned int, Bool, unsigned long);
    Display* display = nullptr;
    FakeKeyEvent fakeKeyEvent = nullptr;
    KeyCode keyCode = 0;
#endif
    bool held = false;

    explicit EscapeInput(NativeWindow& native)
    {
#ifndef _WIN32
        display = native.display;
        // XTest нужен только тесту. Рабочая библиотека платформы от него не зависит.
        native.inputLibrary = dlopen("libXtst.so.6", RTLD_NOW | RTLD_LOCAL);
        if (native.inputLibrary) {
            fakeKeyEvent = reinterpret_cast<FakeKeyEvent>(dlsym(native.inputLibrary, "XTestFakeKeyEvent"));
        }
        keyCode = XKeysymToKeycode(display, XK_Escape);
#else
        (void)native;
#endif
    }

    bool available() const
    {
#ifndef _WIN32
        int opcode = 0;
        int event = 0;
        int error = 0;
        return fakeKeyEvent && keyCode != 0
            && XQueryExtension(display, "XTEST", &opcode, &event, &error);
#else
        return true;
#endif
    }

    bool setHeld(bool down)
    {
#ifdef _WIN32
        // SendInput создаёт системный ввод; после обработки сообщений он виден GetKeyboardState.
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = VK_ESCAPE;
        input.ki.dwFlags = down ? 0U : KEYEVENTF_KEYUP;
        const bool success = SendInput(1, &input, static_cast<int>(sizeof(input))) == 1;
#else
        // XTest меняет серверное состояние, доступное через XQueryKeymap.
        const bool success = fakeKeyEvent(display, keyCode, down ? True : False, CurrentTime) != 0;
        XSync(display, False);
#endif
        if (success) {
            held = down;
        }
        return success;
    }

    ~EscapeInput()
    {
        if (held && !setHeld(false)) {
            std::cerr << "Could not release injected Escape.\n";
        }
    }
};

int runMinimizeOrKeyboard(const std::string& title, bool keyboard)
{
    Platform::WindowOptions options;
    options.width = 400;
    options.height = 300;
    options.title = title;
    std::vector<Platform::Error> errors;
    require(Platform::initialize(options, errors), "Could not initialize the test window.");
    NativeWindow native(title);
    waitFor([](const auto& state) { return state.width == 400 && state.height == 300; },
        "Initial size is incorrect.");
    checkRendering();
    if (keyboard) {
        EscapeInput input(native);
        if (!input.available()) {
            std::cout << "Keyboard test requires the XTest library and server extension.\n";
            shutdown();
            return 77;
        }
        native.focus(true);
        waitFor([](const auto& state) { return state.focused; }, "Focus acquisition failed.");
        require(input.setHeld(true), "Could not press Escape.");
        waitFor([](const auto&) { return Platform::isKeyDown(Platform::Key::Escape); },
            "Injected Escape was not detected by the engine.");
        require(!Platform::isCloseRequested(), "Held Escape must not request shutdown.");
#ifndef _WIN32
        const unsigned int beforeEvents = keyboardQueryCount();
        readState();
        require(keyboardQueryCount() == beforeEvents + 1,
            "Event processing must query the keyboard exactly once while focused.");
        const unsigned int beforeReads = keyboardQueryCount();
        for (const auto key : { Platform::Key::Escape, Platform::Key::Space,
                               Platform::Key::W, Platform::Key::ArrowUp }) {
            (void)Platform::isKeyDown(key);
            (void)Platform::isKeyDown(key);
        }
        require(keyboardQueryCount() == beforeReads,
            "Reading keys must reuse the snapshot without querying X11.");
#endif
        native.focus(false);
        waitFor([](const auto& state) { return !state.focused; }, "Focus loss failed.");
        require(!Platform::isKeyDown(Platform::Key::Escape), "Unfocused window must ignore held keys.");
        native.focus(true);
        waitFor([](const auto& state) {
            return state.focused && Platform::isKeyDown(Platform::Key::Escape);
        }, "Held key was not refreshed after focus acquisition.");
        require(input.setHeld(false), "Could not release Escape.");
        require(Platform::isKeyDown(Platform::Key::Escape),
            "Reading a key must retain the snapshot until event processing.");
        waitFor([](const auto&) { return !Platform::isKeyDown(Platform::Key::Escape); },
            "Released Escape remained pressed.");
        require(!Platform::isCloseRequested(), "Escape must not request shutdown.");
    } else {
        if (!native.minimize()) {
            std::cout << "Minimize test requires a window manager.\n";
            shutdown();
            return 77;
        }
#ifndef _WIN32
        Platform::Error error;
        Platform::WindowState pending;
        require(Platform::getWindowState(pending, error) && !pending.minimized,
            "Reading the snapshot must not consume minimization events.");
#endif
        waitFor([](const auto& state) { return state.minimized && !state.focused; },
            "Minimization was not detected.");
        native.restore();
#ifndef _WIN32
        require(Platform::getWindowState(pending, error) && pending.minimized,
            "Reading the snapshot must not consume restoration events.");
#endif
        waitFor([](const auto& state) { return !state.minimized && state.width == 400 && state.height == 300; },
            "Restoration was not detected.");
        checkRendering();
    }
    shutdown();
    return 0;
}

void runWindow(Platform::WindowOptions options, const std::string& title, bool externalDestroy)
{
    Platform::Error error;
    options.title = title;
    std::vector<Platform::Error> startupErrors;
    const bool success = Platform::initialize(options, startupErrors);
    require(success, startupErrors.empty() ? "Initialization failed." : startupErrors.front().message);
    require(startupErrors.empty(), "Successful initialization must clear previous errors.");
    NativeWindow native(title);
    native.checkTitle(title);
    const std::string changedTitle = u8"Движок — окно 🌌";
    require(Platform::setWindowTitle(changedTitle, error), "Could not set a Unicode title.");
    native.checkTitle(changedTitle);
    require(!Platform::setWindowTitle(std::string("a\0b", 3), error)
        && !Platform::setWindowTitle("\xED\xA0\x80", error), "Invalid titles must be rejected.");
    native.checkTitle(changedTitle);
    require(Platform::setWindowTitle("", error), "Empty titles must be allowed.");
    native.checkTitle("");
    require(Platform::setWindowTitle(title, error), "Could not restore title.");
    Platform::WindowState initialState;
    require(Platform::getWindowState(initialState, error)
        && initialState.width == options.width && initialState.height == options.height
        && !Platform::isCloseRequested(),
        "Window state must be readable before event processing without a close request.");
    waitFor([&](const auto& state) {
        return state.width == options.width && state.height == options.height && !state.minimized
            && !Platform::isCloseRequested();
    }, "Initial client dimensions do not match the requested size.");
    checkRendering();
    native.checkMinimum(options);
    require(!Platform::initialize(options, startupErrors) && startupErrors.size() == 1
        && startupErrors.front().operation == "initialize",
        "Repeated initialization must fail without replacing the window.");
    require(!Platform::isKeyDown(static_cast<Platform::Key>(-1)), "Unknown keys must not map to Space.");
    native.resize();
#ifndef _WIN32
    Platform::WindowState pendingResize;
    require(Platform::getWindowState(pendingResize, error)
        && pendingResize.width == options.width && pendingResize.height == options.height,
        "Reading the snapshot must not consume queued resize events.");
#endif
    waitFor([](const auto& state) { return state.width == 400 && state.height == 300; },
        "Resized client dimensions are incorrect.");
    native.focus(true);
    waitFor([](const auto& state) { return state.focused; }, "Focus acquisition was not detected.");
    native.focus(false);
#ifndef _WIN32
    Platform::WindowState pendingFocus;
    require(Platform::getWindowState(pendingFocus, error) && pendingFocus.focused,
        "Reading the snapshot must not consume queued focus events.");
#endif
    waitFor([](const auto& state) { return !state.focused; }, "Focus loss was not detected.");
    checkRendering();
    if (externalDestroy) {
        native.destroy();
#ifndef _WIN32
        // Пока DestroyNotify не обработано, чтение возвращает прежний снимок без запросов к удалённому окну.
        Platform::WindowState unchanged;
        require(Platform::getWindowState(unchanged, error)
            && unchanged.width == 400 && unchanged.height == 300 && !Platform::isCloseRequested(),
            "Reading the snapshot must not query or detect native destruction.");

        // Изменение заголовка подтверждается синхронно, а снимок остаётся доступным.
        require(!Platform::setWindowTitle("destroyed window", error)
            && error.nativeCode == BadWindow && !error.message.empty(),
            "A native title error must be reported by setWindowTitle.");
        require(Platform::getWindowState(unchanged, error) && unchanged.width == 400,
            "A native title error must not affect reading the snapshot.");
#endif
    } else {
        native.close();
        Platform::WindowState pendingCloseState;
        require(Platform::getWindowState(pendingCloseState, error) && !Platform::isCloseRequested(),
            "Reading native window state must not process the pending close event.");
    }
    waitFor([](const auto&) { return Platform::isCloseRequested(); }, "Close request was not detected.");
    readState();
    require(Platform::isCloseRequested(), "Close request must persist across event processing.");
    shutdown();
}
}

int main(int argumentCount, char* arguments[])
{
#ifndef _WIN32
    const char* displayName = std::getenv("DISPLAY");
    if (!displayName || !*displayName) {
        std::cout << "Window tests require an X11 display.\n";
        return 77;
    }
    const auto processId = getpid();
#else
    const auto processId = GetCurrentProcessId();
#endif
    try {
        const std::string title = u8"Проверка окна 🌌 " + std::to_string(processId);
        if (argumentCount == 2) {
            const std::string scenario = arguments[1];
            require(scenario == "minimize" || scenario == "keyboard", "Unknown test scenario.");
            return runMinimizeOrKeyboard(title, scenario == "keyboard");
        }
        require(argumentCount == 1, "Unexpected test arguments.");
        // Нулевые, независимые и равные начальным минимумы, повторные циклы жизни.
        runWindow({ 320, 240, 0, 0 }, title, false);
        runWindow({ 320, 240, 120, 0 }, title, false);
        runWindow({ 320, 240, 0, 90 }, title, false);
        runWindow({ 320, 240, 320, 240 }, title, true);
        Platform::WindowOptions fixedOptions = { 320, 240, 0, 0, false };
        fixedOptions.title = title;
        std::vector<Platform::Error> startupErrors;
        const bool initialized = Platform::initialize(fixedOptions, startupErrors);
        require(initialized, startupErrors.empty() ? "Initialization failed." : startupErrors.front().message);
        NativeWindow fixedWindow(title);
        fixedWindow.checkMinimum(fixedOptions);
        waitFor([](const auto& state) { return state.width == 320 && state.height == 240; },
            "Fixed client dimensions do not match the requested size.");
        fixedWindow.close();
        waitFor([](const auto&) { return Platform::isCloseRequested(); }, "Fixed window did not close.");
        shutdown();
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        std::vector<Platform::Error> errors;
        Platform::shutdown(errors);
        for (const auto& error : errors) {
            std::cerr << error.operation << ": " << error.message << '\n';
        }
        return 1;
    }
    return 0;
}
