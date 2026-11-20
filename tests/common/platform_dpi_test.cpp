#include "graphics/ui_view.h"
#include "app/platform.h"
#include "platform_test_diagnostics.h"

#ifdef _WIN32
#include <windows.h>
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <GL/glx.h>
#endif
#include <GL/gl.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool nearlyEqual(float a, float b)
{
    return std::abs(a - b) < 0.001F;
}

#ifdef _WIN32
template<class Function>
Function userFunction(const char* name)
{
    return reinterpret_cast<Function>(reinterpret_cast<void (WINAPI*)()>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), name)));
}
using GetDpiContext = HANDLE (WINAPI*)();
using EqualDpiContexts = BOOL (WINAPI*)(HANDLE, HANDLE);
using GetDpi = UINT (WINAPI*)(HWND);
using AdjustFrame = BOOL (WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);

void requestDpi(float scale)
{
    const HWND window = WindowFromDC(wglGetCurrentDC());
    RECT suggested = {};
    require(GetWindowRect(window, &suggested) != FALSE, "Could not read outer bounds.");
    suggested.right += 48;
    suggested.bottom += 32;
    const auto dpi = static_cast<WORD>(std::lround(scale * 96));
    SendMessageW(window, WM_DPICHANGED, MAKEWPARAM(dpi, dpi), reinterpret_cast<LPARAM>(&suggested));
    RECT actual = {};
    require(GetWindowRect(window, &actual) != FALSE && EqualRect(&actual, &suggested), "DPI suggested bounds were ignored.");
}

void resizeWindow()
{
    const HWND window = WindowFromDC(wglGetCurrentDC());
    RECT bounds = { 0, 0, 480, 360 };
    require(userFunction<AdjustFrame>("AdjustWindowRectExForDpi")(&bounds, WS_OVERLAPPEDWINDOW, FALSE, 0,
        userFunction<GetDpi>("GetDpiForWindow")(window)) != FALSE, "Could not calculate resized frame.");
    require(SetWindowPos(window, nullptr, 0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE, "Could not resize window.");
}

void moveMouse()
{
    const HWND window = WindowFromDC(wglGetCurrentDC());
    SendMessageW(window, WM_MOUSEMOVE, 0, MAKELPARAM(125, 75));
}
#else
std::string resources = "Xft.dpi: 144\n";
Atom dpiNotification = None;
int resourceReads = 0;
bool failResourceRead = false;

void notifyDpi()
{
    Display* display = glXGetCurrentDisplay();
    const Window window = glXGetCurrentDrawable();
    dpiNotification = XInternAtom(display, "_ENGINE_TEST_DPI_NOTIFICATION", False);
    XEvent event = {};
    event.xclient.type = ClientMessage;
    event.xclient.display = display;
    event.xclient.window = window;
    event.xclient.message_type = dpiNotification;
    event.xclient.format = 32;
    require(XSendEvent(display, window, False, StructureNotifyMask, &event) != 0, "Could not queue DPI notification.");
    XSync(display, False);
}

void requestDpi(float scale)
{
    resources = "Xft.dpi: " + std::to_string(scale * 96) + "\n";
    notifyDpi();
}

void resizeWindow()
{
    Display* display = glXGetCurrentDisplay();
    XResizeWindow(display, glXGetCurrentDrawable(), 480, 360);
    XSync(display, False);
}

void moveMouse()
{
    Display* display = glXGetCurrentDisplay();
    XEvent event = {};
    event.xmotion.type = MotionNotify;
    event.xmotion.display = display;
    event.xmotion.window = glXGetCurrentDrawable();
    event.xmotion.x = 125;
    event.xmotion.y = 75;
    require(XSendEvent(display, event.xmotion.window, False, PointerMotionMask, &event) != 0, "Could not queue mouse movement.");
    XSync(display, False);
}
#endif

Platform::WindowState process()
{
    Platform::Error error;
    require(PlatformTest::checkOperation(Platform::processEvents(error), error), "Event processing failed.");
    Platform::WindowState state;
    require(Platform::getWindowState(state, error), "Could not read window state.");
    return state;
}

void checkDpiEvent(float scale, bool expected = true)
{
    const auto& events = Platform::getEvents();
    const auto count = std::count_if(events.begin(), events.end(), [](const NeuexpEngine::Event& event) {
        return event.type == NeuexpEngine::EventType::WindowDpiChanged;
    });
    require(count == (expected ? 1 : 0), "Incorrect DPI event count.");
    for (const NeuexpEngine::Event& event : events) {
        if (event.type == NeuexpEngine::EventType::WindowDpiChanged) {
            require(nearlyEqual(event.systemScale, scale), "Incorrect DPI event scale.");
        }
    }
}

void checkViewport(const Platform::WindowState& state)
{
    std::array<GLint, 4> viewport = {};
    glGetIntegerv(GL_VIEWPORT, viewport.data());
    require(viewport == std::array<GLint, 4>{ 0, 0, state.width, state.height }, "Viewport differs from physical client size.");
#ifdef _WIN32
    RECT client = {};
    require(GetClientRect(WindowFromDC(wglGetCurrentDC()), &client) != FALSE
        && client.right == state.width && client.bottom == state.height, "Client dimensions were DPI virtualized.");
#endif
}

void run()
{
    using namespace NeuexpEngine;
    Platform::WindowOptions options;
    options.width = 320;
    options.height = 240;
    options.minimumWidth = 200;
    options.minimumHeight = 150;
    options.title = "DPI test";
    Platform::Error failure;
#ifdef _WIN32
    const auto getContext = userFunction<GetDpiContext>("GetThreadDpiAwarenessContext");
    const auto contextsEqual = userFunction<EqualDpiContexts>("AreDpiAwarenessContextsEqual");
    require(getContext && contextsEqual, "DPI awareness APIs unavailable.");
    const HANDLE originalContext = getContext();
#endif
    const bool initialized = Platform::initialize(options, failure);
    PlatformTest::reportError(failure);
    require(initialized, "DPI platform initialization failed.");
    Platform::WindowState state = process();
    require(state.width == 320 && state.height == 240, "Initial client size changed with DPI.");
    checkViewport(state);
#ifdef _WIN32
    using GetAwareness = int (WINAPI*)(HANDLE);
    require(userFunction<GetAwareness>("GetAwarenessFromDpiAwarenessContext")(getContext()) == 2, "Thread is not per-monitor DPI aware.");
    require(nearlyEqual(state.systemScale, static_cast<float>(userFunction<GetDpi>("GetDpiForWindow")(
        WindowFromDC(wglGetCurrentDC()))) / 96), "Initial DPI differs from native DPI.");
#else
    require(nearlyEqual(state.systemScale, 1.5F), "Initial Xft.dpi was ignored.");
    const int reads = resourceReads;
    Platform::WindowState cached;
    Platform::Error error;
    resources = "Xft.dpi: 192\n";
    require(Platform::getWindowState(cached, error) && nearlyEqual(cached.systemScale, 1.5F)
        && resourceReads == reads, "Window snapshot queried DPI or changed without an event.");
#endif

    UiCanvas canvas = { state.systemScale };
    canvas.scale = 1.25F;
    Camera2 camera;
    camera.position = { 500, 300 };
    camera.zoom = 1.5F;
    UiView ui;
    UiError uiError;
    CameraView world;
    CameraError cameraError;
    for (float scale : { 2.0F, 1.25F, 1.0F, 1.5F }) {
        requestDpi(scale);
        state = process();
        require(nearlyEqual(state.systemScale, scale), "DPI snapshot did not update.");
        checkDpiEvent(scale);
        checkViewport(state);
#ifdef _WIN32
        RECT minimum = { 0, 0, options.minimumWidth, options.minimumHeight };
        require(userFunction<AdjustFrame>("AdjustWindowRectExForDpi")(&minimum, WS_OVERLAPPEDWINDOW, FALSE, 0,
            static_cast<UINT>(std::lround(scale * 96))) != FALSE, "Could not calculate DPI minimum frame.");
        MINMAXINFO limits = {};
        SendMessageW(WindowFromDC(wglGetCurrentDC()), WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&limits));
        require(limits.ptMinTrackSize.x == minimum.right - minimum.left
            && limits.ptMinTrackSize.y == minimum.bottom - minimum.top, "DPI minimum client size is incorrect.");
#endif
        require(canvas.createView({ 0, 0, state.width, state.height }, ui, uiError)
            && nearlyEqual(ui.uiToScreen({ 100, 60 }).x, 125) && nearlyEqual(ui.uiToScreen({ 100, 60 }).y, 75)
            && canvas.scale == 1.25F, "DPI change replaced chosen UI scale.");
        require(camera.createView({ 0, 0, state.width, state.height }, world, cameraError)
            && camera.zoom == 1.5F, "DPI change replaced world zoom.");
        Vector2 point;
        require(world.screenToWorld({ static_cast<float>(state.width) / 2, static_cast<float>(state.height) / 2 }, point)
            && nearlyEqual(point.x, 500) && nearlyEqual(point.y, 300), "World mouse mapping after DPI failed.");
    }
    requestDpi(1.5F);
    state = process();
    checkDpiEvent(1.5F, false);
    checkViewport(state);

    resizeWindow();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    do {
        state = process();
        if (state.width == 480 && state.height == 360) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < deadline);
    require(state.width == 480 && state.height == 360 && nearlyEqual(state.systemScale, 1.5F), "Resize lost DPI or used logical pixels.");
    checkViewport(state);
    moveMouse();
    process();
    require(canvas.createView({ 0, 0, state.width, state.height }, ui, uiError), "Resized UI failed.");
    const auto& events = Platform::getEvents();
    const auto mouse = std::find_if(events.begin(), events.end(), [](const Event& event) {
        return event.type == EventType::MouseMoved && event.x == 125 && event.y == 75;
    });
    Vector2 logical;
    require(mouse != events.end() && ui.screenToUi({ static_cast<float>(mouse->x), static_cast<float>(mouse->y) }, logical)
        && nearlyEqual(logical.x, 100) && nearlyEqual(logical.y, 60), "Mouse coordinates after DPI and resize are incorrect.");

#ifndef _WIN32
    for (const std::string invalid : { "", "Xft.dpi: -96\n", "Xft.dpi: 0\n", "Xft.dpi: nan\n",
        "Xft.dpi: inf\n", "Xft.dpi: 144oops\n", "Xft.dpi: 1e40\n" }) {
        resources = invalid;
        notifyDpi();
        state = process();
        require(nearlyEqual(state.systemScale, 1), "Invalid desktop DPI did not use the default.");
    }
    failResourceRead = true;
    notifyDpi();
    require(!Platform::processEvents(error) && error.operation == "XGetTextProperty (RESOURCE_MANAGER)", "Native DPI read failure was ignored.");
    require(Platform::getWindowState(cached, error) && nearlyEqual(cached.systemScale, 1), "DPI read failure changed snapshot.");
    failResourceRead = false;
    requestDpi(1.5F);
    require(nearlyEqual(process().systemScale, 1.5F), "DPI failed to recover.");
#endif
    Platform::shutdown(failure);
    PlatformTest::reportError(failure);
    require(failure.operation.empty(), "DPI platform shutdown failed.");
#ifdef _WIN32
    require(contextsEqual(getContext(), originalContext) != FALSE, "Thread DPI context was not restored.");
    options.resizable = false;
    require(Platform::initialize(options, failure), "Fixed DPI window initialization failed.");
    requestDpi(1.5F);
    state = process();
    checkViewport(state);
    const HWND fixedWindow = WindowFromDC(wglGetCurrentDC());
    MINMAXINFO fixedLimits = {};
    SendMessageW(fixedWindow, WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&fixedLimits));
    RECT fixedBounds = {};
    require(GetWindowRect(fixedWindow, &fixedBounds) != FALSE, "Could not read fixed window bounds.");
    require(fixedLimits.ptMinTrackSize.x == fixedBounds.right - fixedBounds.left
        && fixedLimits.ptMinTrackSize.y == fixedBounds.bottom - fixedBounds.top
        && fixedLimits.ptMaxTrackSize.x == fixedLimits.ptMinTrackSize.x
        && fixedLimits.ptMaxTrackSize.y == fixedLimits.ptMinTrackSize.y, "Fixed window resizing limits lost after DPI change.");
    Platform::shutdown(failure);
    require(failure.operation.empty() && contextsEqual(getContext(), originalContext) != FALSE, "Fixed DPI shutdown failed.");
    WNDCLASSW occupied = {};
    occupied.lpfnWndProc = DefWindowProcW;
    occupied.hInstance = GetModuleHandleW(nullptr);
    occupied.lpszClassName = L"EngineWindow";
    require(RegisterClassW(&occupied) != 0, "Could not arrange initialization failure.");
    const bool failed = !Platform::initialize(options, failure);
    const bool restored = contextsEqual(getContext(), originalContext) != FALSE;
    require(UnregisterClassW(occupied.lpszClassName, occupied.hInstance) != FALSE, "Could not remove occupied class.");
    require(failed && restored, "Failed initialization did not restore DPI context.");
#endif
}
}

#ifndef _WIN32
extern "C" Status __real_XGetTextProperty(Display*, Window, XTextProperty*, Atom);
extern "C" Status __wrap_XGetTextProperty(Display* display, Window window, XTextProperty* property, Atom atom)
{
    if (window == DefaultRootWindow(display) && atom == XA_RESOURCE_MANAGER) {
        ++resourceReads;
        if (failResourceRead) {
            return __real_XGetTextProperty(display, None, property, atom);
        }
        char* value = resources.data();
        return XStringListToTextProperty(&value, 1, property);
    }
    return __real_XGetTextProperty(display, window, property, atom);
}

extern "C" int __real_XNextEvent(Display*, XEvent*);
extern "C" int __wrap_XNextEvent(Display* display, XEvent* event)
{
    const int result = __real_XNextEvent(display, event);
    // Уведомление только в соединении теста; свойства рабочего стола не меняются.
    if (event->type == ClientMessage && event->xclient.message_type == dpiNotification) {
        *event = {};
        event->xproperty.type = PropertyNotify;
        event->xproperty.display = display;
        event->xproperty.window = DefaultRootWindow(display);
        event->xproperty.atom = XA_RESOURCE_MANAGER;
    }
    return result;
}
#endif

int main()
{
#ifndef _WIN32
    if (!std::getenv("DISPLAY") || !*std::getenv("DISPLAY")) {
        return 77;
    }
#endif
    try {
        run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        Platform::Error failure;
        Platform::shutdown(failure);
        PlatformTest::reportError(failure);
        return 1;
    }
}
