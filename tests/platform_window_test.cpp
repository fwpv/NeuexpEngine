#include "platform.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#include <unistd.h>
#endif

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
    require(result != Platform::EventResult::Failed, error.operation + ": " + error.message);
    Platform::WindowState state;
    const bool success = Platform::getWindowState(state, error);
    require(success, error.operation + ": " + error.message);
    require((result == Platform::EventResult::Quit) == Platform::isCloseRequested(),
        "Event result and the stored close request disagree.");
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

    explicit NativeWindow(const std::string& title)
    {
        handle = FindWindowA("OpenGLDemoWindow", title.c_str());
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

    Window findWindow(Window parent, const std::string& title)
    {
        char* name = nullptr;
        XFetchName(display, parent, &name);
        const bool found = name && title == name;
        if (name) {
            XFree(name);
        }
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

void runWindow(const Platform::WindowOptions& options, const std::string& title, bool externalDestroy)
{
    Platform::Error error;
    const bool success = Platform::initialize(options, error);
    require(success, error.operation + ": " + error.message);
    require(Platform::setWindowTitle(title.c_str(), error), "Could not set window title.");
    require(!Platform::setWindowTitle(nullptr, error), "A null title must fail safely.");
    NativeWindow native(title);
    Platform::WindowState initialState;
    require(Platform::getWindowState(initialState, error)
        && initialState.width == options.width && initialState.height == options.height
        && !Platform::isCloseRequested(),
        "Window state must be readable before event processing without a close request.");
    waitFor([&](const auto& state) {
        return state.width == options.width && state.height == options.height && !state.minimized
            && !Platform::isCloseRequested();
    }, "Initial client dimensions do not match the requested size.");
    native.checkMinimum(options);
    require(!Platform::initialize(options, error) && error.operation == "initialize",
        "Repeated initialization must fail without replacing the window.");
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
    if (native.minimize()) {
#ifndef _WIN32
        Platform::WindowState pendingMinimize;
        require(Platform::getWindowState(pendingMinimize, error) && !pendingMinimize.minimized,
            "Reading the snapshot must not query the new minimized state.");
#endif
        waitFor([](const auto& state) { return state.minimized && !state.focused; },
            "Minimization was not detected.");
        native.restore();
#ifndef _WIN32
        Platform::WindowState pendingRestore;
        require(Platform::getWindowState(pendingRestore, error) && pendingRestore.minimized,
            "Reading the snapshot must not query the new restored state.");
#endif
        waitFor([](const auto& state) { return !state.minimized && state.width == 400 && state.height == 300; },
            "Restoration was not detected.");
    } else {
        std::cout << "Minimize/restore scenario skipped: no window manager manages the test window.\n";
    }
    require(Platform::present(error), "Present failed after window changes.");
    if (externalDestroy) {
        native.destroy();
#ifndef _WIN32
        // Пока DestroyNotify не обработано, чтение возвращает прежний снимок без запросов к удалённому окну.
        Platform::WindowState unchanged;
        require(Platform::getWindowState(unchanged, error)
            && unchanged.width == 400 && unchanged.height == 300 && !Platform::isCloseRequested(),
            "Reading the snapshot must not query or detect native destruction.");

        // Запрос к удалённому окну создаёт настоящую ошибку X11. Доставляем её обработчику
        // через текущее GLX-соединение теста и проверяем раздельное чтение снимка и ошибок.
        require(Platform::setWindowTitle("destroyed window", error), "The X11 title request should be asynchronous.");
        XSync(glXGetCurrentDisplay(), False);
        require(Platform::getWindowState(unchanged, error) && unchanged.width == 400,
            "A pending native error must not affect reading the snapshot.");
        require(Platform::processEvents(error) == Platform::EventResult::Failed
            && error.nativeCode == BadWindow && !error.message.empty(),
            "Native errors must be reported by event processing.");
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

int main()
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
        const std::string title = "Platform window test " + std::to_string(processId);
        // Нулевые, независимые и равные начальным минимумы, повторные циклы жизни.
        runWindow({ 320, 240, 0, 0 }, title, false);
        runWindow({ 320, 240, 120, 0 }, title, false);
        runWindow({ 320, 240, 0, 90 }, title, false);
        runWindow({ 320, 240, 320, 240 }, title, true);
        const Platform::WindowOptions fixedOptions = { 320, 240, 0, 0, false };
        Platform::Error error;
        const bool initialized = Platform::initialize(fixedOptions, error);
        require(initialized, error.operation + ": " + error.message);
        require(Platform::setWindowTitle(title.c_str(), error), "Could not set fixed window title.");
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
