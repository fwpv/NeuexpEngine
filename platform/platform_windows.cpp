#include "platform.h"
#include "detail/cleanup_error.h"
#include "window_validation.h"
#include "key_codes.h"
#include "extension_names.h"
#include "detail/platform_input.h"
#include "detail/unicode.h"
#include "input.h"

#include <windows.h>
#include <mmsystem.h>
#include <GL/gl.h>
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>
#include <cstddef>
#include <optional>
#include <string>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

namespace Platform
{
namespace
{
HWND window = nullptr;
HDC deviceContext = nullptr;
HGLRC openGLContext = nullptr;

// Успешный запрос timeBeginPeriod(1).
bool timerResolutionEnabled = false;

HINSTANCE applicationInstance = nullptr;
constexpr const wchar_t* WindowClassName = L"EngineWindow";
bool windowClassRegistered = false;
bool closeRequested = false;
bool initialized = false;
WindowState windowState;
bool viewportNeedsUpdate = false;
std::array<bool, static_cast<std::size_t>(Key::Count)> heldKeys = {};
unsigned int heldMouseButtons = 0;
bool mouseInside = false;
std::uint32_t highSurrogate = 0;
WindowOptions windowOptions;
LONG minimumTrackWidth = 0;
LONG minimumTrackHeight = 0;
std::optional<Error> pendingInputError;
using SetDpiContext = HANDLE (WINAPI*)(HANDLE);
using GetWindowDpi = UINT (WINAPI*)(HWND);
using AdjustDpiFrame = BOOL (WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
using EnableDpiFrame = BOOL (WINAPI*)(HWND);
SetDpiContext setDpiContext = nullptr;
GetWindowDpi getWindowDpi = nullptr;
AdjustDpiFrame adjustDpiFrame = nullptr;
EnableDpiFrame enableDpiFrame = nullptr;
HANDLE previousDpiContext = nullptr;

template<class Function>
Function loadUserFunction(const char* name)
{
    const auto address = GetProcAddress(GetModuleHandleW(L"user32.dll"), name);
    return reinterpret_cast<Function>(reinterpret_cast<void (WINAPI*)()>(address));
}

template<class Function>
Function loadWglFunction(const char* name)
{
    const PROC address = wglGetProcAddress(name);
    const auto value = reinterpret_cast<std::intptr_t>(address);
    if (!address || value == 1 || value == 2 || value == 3 || value == -1) {
        return nullptr;
    }
    return reinterpret_cast<Function>(reinterpret_cast<void (WINAPI*)()>(address));
}

Error windowsError(const char* operation, const char* message, DWORD code = ERROR_SUCCESS)
{
    Error error = {operation, message, std::nullopt};
    if (code != ERROR_SUCCESS) {
        error.nativeCode = static_cast<int>(code);
        // Буфер FormatMessageW освобождается через LocalFree.
        wchar_t* description = nullptr;
        const DWORD descriptionLength = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, code, 0, reinterpret_cast<LPWSTR>(&description), 0, nullptr);
        const std::unique_ptr<wchar_t, decltype(&LocalFree)> descriptionOwner(description, LocalFree);
        if (descriptionLength != 0) {
            const int length = WideCharToMultiByte(CP_UTF8, 0, description,
                static_cast<int>(descriptionLength), nullptr, 0, nullptr, nullptr);
            if (length > 0) {
                std::string utf8Description(static_cast<std::size_t>(length), '\0');
                if (WideCharToMultiByte(CP_UTF8, 0, description,
                    static_cast<int>(descriptionLength), utf8Description.data(), length, nullptr, nullptr) != 0) {
                    error.message += " ";
                    error.message += utf8Description;
                }
            }
        }
    }
    return error;
}

bool enableDpiAwareness(Error& error)
{
    enableDpiFrame = nullptr;
    setDpiContext = loadUserFunction<SetDpiContext>("SetThreadDpiAwarenessContext");
    getWindowDpi = loadUserFunction<GetWindowDpi>("GetDpiForWindow");
    adjustDpiFrame = loadUserFunction<AdjustDpiFrame>("AdjustWindowRectExForDpi");
    if (!setDpiContext || !getWindowDpi || !adjustDpiFrame) {
        error = windowsError("DPI awareness", "Per-monitor DPI support requires Windows 10 version 1607 or later.");
        return false;
    }
    // PMv2 доступен с Windows 10 1703, PMv1 — с 1607. Контекст потока восстанавливается в shutdown().
    previousDpiContext = setDpiContext(reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-4)));
    if (!previousDpiContext && GetLastError() == ERROR_INVALID_PARAMETER) {
        enableDpiFrame = loadUserFunction<EnableDpiFrame>("EnableNonClientDpiScaling");
        if (!enableDpiFrame) {
            error = windowsError("EnableNonClientDpiScaling", "Per-monitor window frame scaling is unavailable.");
            return false;
        }
        previousDpiContext = setDpiContext(reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-3)));
    }
    if (!previousDpiContext) {
        error = windowsError("SetThreadDpiAwarenessContext", "Could not enable per-monitor DPI awareness.", GetLastError());
        return false;
    }
    return true;
}

DWORD windowStyle()
{
    return windowOptions.resizable ? WS_OVERLAPPEDWINDOW
        : WS_OVERLAPPEDWINDOW & ~static_cast<DWORD>(WS_THICKFRAME | WS_MAXIMIZEBOX);
}

bool updateMinimumTrackSize(UINT dpi, Error& error)
{
    RECT bounds = {0, 0, windowOptions.resizable ? windowOptions.minimumWidth : windowOptions.width,
        windowOptions.resizable ? windowOptions.minimumHeight : windowOptions.height};
    if (!adjustDpiFrame(&bounds, windowStyle(), FALSE, 0, dpi)) {
        error = windowsError("AdjustWindowRectExForDpi", "Could not calculate the minimum frame size.", GetLastError());
        return false;
    }
    minimumTrackWidth = bounds.right - bounds.left;
    minimumTrackHeight = bounds.bottom - bounds.top;
    return true;
}

// Оконная процедура сохраняет первую ошибку до processEvents или shutdown.
void recordInputError(const char* operation, const char* message, DWORD code = ERROR_SUCCESS)
{
    if (!pendingInputError) {
        pendingInputError = windowsError(operation, message, code);
    }
}

bool takeInputError(Error& error)
{
    if (!pendingInputError) {
        return true;
    }
    error = std::move(*pendingInputError);
    pendingInputError.reset();
    return false;
}

bool convertTitle(const std::string& title, std::wstring& wideTitle, Error& error)
{
    if (title.empty()) {
        wideTitle.clear();
        return true;
    }
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title.data(),
        static_cast<int>(title.size()), nullptr, 0);
    if (length == 0) {
        error = windowsError("MultiByteToWideChar", "Could not convert the window title.", GetLastError());
        return false;
    }
    wideTitle.resize(static_cast<std::size_t>(length));
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title.data(),
        static_cast<int>(title.size()), wideTitle.data(), length) == 0) {
        error = windowsError("MultiByteToWideChar", "Could not convert the window title.", GetLastError());
        return false;
    }
    return true;
}

Engine::Modifiers readModifiers()
{
    const auto held = [](Key key) { return heldKeys[static_cast<std::size_t>(key)]; };
    return {held(Key::LeftShift) || held(Key::RightShift),
        held(Key::LeftControl) || held(Key::RightControl),
        held(Key::LeftAlt) || held(Key::RightAlt), held(Key::LeftSuper) || held(Key::RightSuper),
        (GetKeyState(VK_CAPITAL) & 1) != 0, (GetKeyState(VK_NUMLOCK) & 1) != 0};
}

void changeFocus(bool focused)
{
    if (windowState.focused == focused) {
        return;
    }
    windowState.focused = focused;
    Engine::Event event;
    event.type = focused ? Engine::EventType::FocusGained : Engine::EventType::FocusLost;
    if (!focused) {
        heldKeys = {};
        heldMouseButtons = 0;
        highSurrogate = 0;
        if (window && GetCapture() == window && !ReleaseCapture()) {
            recordInputError("ReleaseCapture", "Could not release mouse capture.", GetLastError());
        }
    }
    event.modifiers = readModifiers();
    Detail::queueEvent(event);
}

Key translateKey(WPARAM virtualKey, LPARAM data)
{
    if (virtualKey == VK_NUMLOCK) {
        return Key::NumLock; // NumLock может иметь E0-флаг, совпадающий с кодом Pause.
    }
    if (virtualKey == VK_PAUSE) {
        return Key::Pause;
    }
    if (virtualKey == VK_SNAPSHOT) {
        return Key::PrintScreen;
    }
    // Бит 24 отличает правый Ctrl, Alt, цифровой Enter и навигационный блок.
    const auto scan = static_cast<unsigned int>((data >> 16) & 0x1ff);
    for (const auto& mapping : Detail::KeyMappings) {
        if (mapping.scan == scan) {
            return mapping.key;
        }
    }
    return Key::Unknown;
}

void queueText(std::uint32_t code)
{
    // Tab, Enter, Backspace и Ctrl-команды обрабатываются как клавиши, а не вставляемый текст.
    if (code < 0x20 || (code >= 0x7f && code <= 0x9f)) {
        return;
    }
    Engine::Event event;
    event.type = Engine::EventType::TextInput;
    event.modifiers = readModifiers();
    Engine::Detail::appendUtf8(event.text, code);
    Detail::queueEvent(event);
}

bool handleInput(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{
    using Engine::EventType;
    using Engine::MouseButton;
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN || message == WM_KEYUP || message == WM_SYSKEYUP) {
        if (windowState.focused) {
            Engine::Event event;
            const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
            event.type = down ? EventType::KeyPressed : EventType::KeyReleased;
            event.key = translateKey(wParam, lParam);
            heldKeys[static_cast<std::size_t>(event.key)] = down;
            event.modifiers = readModifiers();
            Detail::queueEvent(event);
        }
        // Системные сочетания, включая Alt+F4, должны пройти DefWindowProc.
        return message == WM_KEYDOWN || message == WM_KEYUP;
    }
    if (message == WM_CHAR || message == WM_UNICHAR) {
        if (windowState.focused) {
            const auto code = static_cast<std::uint32_t>(wParam);
            if (message == WM_UNICHAR) {
                if (highSurrogate != 0) {
                    queueText(0xfffd);
                    highSurrogate = 0;
                }
                const unsigned int repeats = LOWORD(lParam) == 0 ? 1 : LOWORD(lParam);
                for (unsigned int index = 0; index < repeats; ++index) {
                    queueText(code);
                }
            } else {
                const unsigned int repeats = LOWORD(lParam) == 0 ? 1 : LOWORD(lParam);
                for (unsigned int index = 0; index < repeats; ++index) {
                    if (highSurrogate != 0) {
                        if (code >= 0xdc00 && code <= 0xdfff) {
                            queueText(0x10000 + ((highSurrogate - 0xd800) << 10) + code - 0xdc00);
                            highSurrogate = 0;
                            continue;
                        }
                        queueText(0xfffd);
                        highSurrogate = 0;
                    }
                    if (code >= 0xd800 && code <= 0xdbff) {
                        highSurrogate = code;
                    } else {
                        queueText(code);
                    }
                }
            }
        }
        return true;
    }
    if (message == WM_CAPTURECHANGED || message == WM_CANCELMODE) {
        if (heldMouseButtons != 0) {
            heldMouseButtons = 0;
            Detail::queueEvent(EventType::MouseCaptureLost);
        }
        if (message == WM_CANCELMODE && GetCapture() == handle && !ReleaseCapture()) {
            recordInputError("ReleaseCapture", "Could not release mouse capture.", GetLastError());
        }
        return false;
    }
    if (message == WM_MOUSELEAVE) {
        if (mouseInside) {
            mouseInside = false;
            Detail::queueEvent(EventType::MouseLeft);
        }
        return true;
    }
    Engine::Event event;
    event.x = static_cast<short>(LOWORD(lParam));
    event.y = static_cast<short>(HIWORD(lParam));
    event.modifiers = readModifiers();
    if (message == WM_MOUSEMOVE) {
        const bool inside = event.x >= 0 && event.y >= 0
            && event.x < windowState.width && event.y < windowState.height;
        if (inside != mouseInside) {
            mouseInside = inside;
            event.type = inside ? EventType::MouseEntered : EventType::MouseLeft;
            Detail::queueEvent(event);
        }
        if (inside) {
            // Уведомление TME_LEAVE одноразовое; запрос повторяется после входа.
            TRACKMOUSEEVENT tracking = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, handle, 0};
            if (!TrackMouseEvent(&tracking)) {
                recordInputError("TrackMouseEvent", "Could not track the mouse leaving the window.", GetLastError());
            }
        }
        event.type = EventType::MouseMoved;
    } else if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) {
        POINT position = {event.x, event.y};
        // Колесо, в отличие от кнопок, использует экранные координаты.
        if (!ScreenToClient(handle, &position)) {
            recordInputError("ScreenToClient", "Could not convert the mouse wheel position to client coordinates.");
            return true;
        }
        event.x = position.x;
        event.y = position.y;
        event.type = EventType::MouseWheel;
        const double amount = static_cast<short>(HIWORD(wParam)) / static_cast<double>(WHEEL_DELTA);
        event.wheelX = message == WM_MOUSEHWHEEL ? amount : 0;
        event.wheelY = message == WM_MOUSEWHEEL ? amount : 0;
    } else {
        bool down = false;
        switch (message) {
            case WM_LBUTTONDOWN: down = true; event.button = MouseButton::Left; break;
            case WM_LBUTTONUP: event.button = MouseButton::Left; break;
            case WM_RBUTTONDOWN: down = true; event.button = MouseButton::Right; break;
            case WM_RBUTTONUP: event.button = MouseButton::Right; break;
            case WM_MBUTTONDOWN: down = true; event.button = MouseButton::Middle; break;
            case WM_MBUTTONUP: event.button = MouseButton::Middle; break;
            case WM_XBUTTONDOWN: down = true; [[fallthrough]];
            case WM_XBUTTONUP:
                event.button = HIWORD(wParam) == XBUTTON1 ? MouseButton::Back : MouseButton::Forward;
                break;
            default: return false;
        }
        if (!windowState.focused) {
            return true;
        }
        const unsigned int bit = 1U << static_cast<unsigned int>(event.button);
        if (down) {
            SetCapture(handle); // Получаем отпускание и за пределами окна при drag.
            if (GetCapture() != handle) {
                recordInputError("SetCapture", "Could not capture the mouse for dragging.");
                return true;
            }
            heldMouseButtons |= bit;
        } else {
            heldMouseButtons &= ~bit;
        }
        event.type = down ? EventType::MousePressed : EventType::MouseReleased;
        Detail::queueEvent(event);
        if (!down && heldMouseButtons == 0 && GetCapture() == handle && !ReleaseCapture()) {
            recordInputError("ReleaseCapture", "Could not release mouse capture.", GetLastError());
        }
        return true;
    }
    Detail::queueEvent(event);
    return true;
}

LRESULT CALLBACK windowProcedure(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Для PMv1 рамка включается явно, в PMv2 Windows масштабирует её автоматически.
    if (message == WM_NCCREATE && enableDpiFrame && !enableDpiFrame(handle)) {
        recordInputError("EnableNonClientDpiScaling", "Could not enable per-monitor window frame scaling.", GetLastError());
        return FALSE;
    }
    if (message == WM_UNICHAR && wParam == UNICODE_NOCHAR) {
        return TRUE; // Проверка поддержки UTF-32 отправителем.
    }
    if (handleInput(handle, message, wParam, lParam)) {
        return message == WM_XBUTTONDOWN || message == WM_XBUTTONUP ? TRUE : 0;
    }
    // WM_GETMINMAXINFO задаёт ограничения внешней рамки, хотя API движка принимает клиентские размеры.
    if (message == WM_GETMINMAXINFO) {
        const LRESULT result = DefWindowProcW(handle, message, wParam, lParam);
        auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
        if (!windowOptions.resizable || windowOptions.minimumWidth > 0) {
            limits->ptMinTrackSize.x = minimumTrackWidth;
        }
        if (!windowOptions.resizable || windowOptions.minimumHeight > 0) {
            limits->ptMinTrackSize.y = minimumTrackHeight;
        }
        if (!windowOptions.resizable) {
            limits->ptMaxTrackSize.x = minimumTrackWidth;
            limits->ptMaxTrackSize.y = minimumTrackHeight;
        }
        return result;
    }

    if (message == WM_DPICHANGED) {
        const UINT dpi = LOWORD(wParam);
        const auto& suggested = *reinterpret_cast<const RECT*>(lParam);
        if (!windowOptions.resizable) {
            RECT frame = {};
            if (!adjustDpiFrame(&frame, windowStyle(), FALSE, 0, dpi)) {
                recordInputError("AdjustWindowRectExForDpi", "Could not calculate the fixed window frame.", GetLastError());
                return 0;
            }
            // Запрет ручного изменения размера сохраняется и после системного изменения DPI.
            windowOptions.width = suggested.right - suggested.left - (frame.right - frame.left);
            windowOptions.height = suggested.bottom - suggested.top - (frame.bottom - frame.top);
        }
        Error error;
        if (!updateMinimumTrackSize(dpi, error)) {
            if (!pendingInputError) {
                pendingInputError = std::move(error);
            }
            return 0;
        }
        const float scale = static_cast<float>(dpi) / 96;
        if (windowState.systemScale != scale) {
            windowState.systemScale = scale;
            Engine::Event event;
            event.type = Engine::EventType::WindowDpiChanged;
            event.systemScale = scale;
            Detail::queueEvent(event);
        }
        if (!SetWindowPos(handle, nullptr, suggested.left, suggested.top,
            suggested.right - suggested.left, suggested.bottom - suggested.top, SWP_NOZORDER | SWP_NOACTIVATE)) {
            recordInputError("SetWindowPos (WM_DPICHANGED)", "Could not apply the DPI window rectangle.", GetLastError());
        }
        return 0;
    }

    if (message == WM_SIZE) {
        const int width = static_cast<int>(LOWORD(lParam));
        const int height = static_cast<int>(HIWORD(lParam));
        viewportNeedsUpdate = viewportNeedsUpdate || width != windowState.width || height != windowState.height;
        if (width != windowState.width || height != windowState.height) {
            Engine::Event event;
            event.type = Engine::EventType::WindowResized;
            event.width = width;
            event.height = height;
            Detail::queueEvent(event);
        }
        const bool minimized = wParam == SIZE_MINIMIZED;
        if (minimized != windowState.minimized) {
            Detail::queueEvent(minimized ? Engine::EventType::WindowMinimized : Engine::EventType::WindowRestored);
        }
        windowState.width = width;
        windowState.height = height;
        windowState.minimized = wParam == SIZE_MINIMIZED;
        if (windowState.minimized) {
            changeFocus(false);
        }
        return 0;
    }
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS) {
        changeFocus(message == WM_SETFOCUS && !windowState.minimized);
        return 0;
    }

    // WM_CLOSE только запрашивает выход. Контекст и HDC освобождаются до уничтожения окна в shutdown().
    if (message == WM_CLOSE) {
        closeRequested = true;
        Detail::queueEvent(Engine::EventType::CloseRequested);
        return 0;
    }

    if (message == WM_DESTROY) {
        if (handle == window) {
            changeFocus(false);
            Detail::queueEvent(Engine::EventType::WindowDestroyed);
            if (!closeRequested) {
                Detail::queueEvent(Engine::EventType::CloseRequested);
            }
            window = nullptr;
            deviceContext = nullptr; // При внешнем уничтожении окна его частный DC уже недоступен.
            closeRequested = true;
            windowState = {};
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(handle, message, wParam, lParam);
}

bool requireWindow(const char* operation, Error& error)
{
    if (!initialized || !window) {
        error = {operation, "The window is not initialized or has been destroyed.", std::nullopt};
        return false;
    }
    return true;
}

bool initializeWindows(const WindowOptions& options, Error& error)
{
    std::wstring title;
    if (!convertTitle(options.title, title, error)) {
        return false;
    }
    windowOptions = options;
    Detail::resetInput();
    windowState = {};
    if (!enableDpiAwareness(error)) {
        return false;
    }
    const DWORD style = windowStyle();
    RECT bounds = {0, 0, options.width, options.height};
    if (!adjustDpiFrame(&bounds, style, FALSE, 0, 96)) {
        error = windowsError("AdjustWindowRectExForDpi", "Could not calculate the window frame size.", GetLastError());
        return false;
    }
    if (!updateMinimumTrackSize(96, error)) {
        return false;
    }
    // shutdown может оставить WM_QUIT от уничтоженного окна в очереди текущего потока.
    MSG oldQuit;
    while (PeekMessageW(&oldQuit, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) {
    }
    closeRequested = false;
    applicationInstance = GetModuleHandle(nullptr);
    if (!applicationInstance) {
        error = windowsError("GetModuleHandle", "Could not get the application module.", GetLastError());
        return false;
    }

    WNDCLASSW windowClass = {};
    // CS_OWNDC закрепляет постоянный HDC за окном.
    windowClass.style = CS_OWNDC;
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = applicationInstance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = WindowClassName;
    if (!RegisterClassW(&windowClass)) {
        error = windowsError("RegisterClass", "Could not register the window class.", GetLastError());
        return false;
    }

    windowClassRegistered = true;

    // Фиксированное окно не имеет растягиваемой рамки и кнопки разворачивания.
    window = CreateWindowW(windowClass.lpszClassName, title.c_str(),
        style, 100, 100, bounds.right - bounds.left, bounds.bottom - bounds.top,
        nullptr, nullptr, applicationInstance, nullptr);
    if (!window) {
        error = windowsError("CreateWindow", "Could not create the application window.", GetLastError());
        return false;
    }

    const UINT dpi = getWindowDpi(window);
    if (dpi == 0) {
        error = windowsError("GetDpiForWindow", "Could not read the initial window DPI.");
        return false;
    }
    if (!updateMinimumTrackSize(dpi, error)) {
        return false;
    }
    if (dpi != 96) {
        bounds = {0, 0, options.width, options.height};
        if (!adjustDpiFrame(&bounds, style, FALSE, 0, dpi)) {
            error = windowsError("AdjustWindowRectExForDpi", "Could not calculate the initial DPI frame size.", GetLastError());
            return false;
        }
        if (!SetWindowPos(window, nullptr, 0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE)) {
            error = windowsError("SetWindowPos", "Could not set the physical client size.", GetLastError());
            return false;
        }
    }

    deviceContext = GetDC(window);
    if (!deviceContext) {
        // GetDC не документирует расширенную ошибку через GetLastError.
        error = windowsError("GetDC", "Could not get the window drawing surface.");
        return false;
    }

    PIXELFORMATDESCRIPTOR format = {};
    format.nSize = sizeof(format);
    format.nVersion = 1;
    format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    format.iPixelType = PFD_TYPE_RGBA;
    format.cColorBits = 32;
    format.cDepthBits = 0;
    const int pixelFormat = ChoosePixelFormat(deviceContext, &format);

    if (pixelFormat == 0) {
        error = windowsError("ChoosePixelFormat", "No compatible OpenGL pixel format was found.", GetLastError());
        return false;
    }
    PIXELFORMATDESCRIPTOR actualFormat = {};
    if (!DescribePixelFormat(deviceContext, pixelFormat, sizeof(actualFormat), &actualFormat)) {
        error = windowsError("DescribePixelFormat", "Could not inspect the selected pixel format.", GetLastError());
        return false;
    }
    constexpr DWORD RequiredPixelFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    // Разрядность цвета — предпочтение запроса, а не условие отказа.
    if ((actualFormat.dwFlags & RequiredPixelFlags) != RequiredPixelFlags
        || actualFormat.iPixelType != PFD_TYPE_RGBA) {
        error = windowsError("ChoosePixelFormat", "The selected format requires window OpenGL, double buffering and RGBA colors.");
        return false;
    }
    if (!SetPixelFormat(deviceContext, pixelFormat, &format)) {
        error = windowsError("SetPixelFormat", "Could not set the OpenGL pixel format.", GetLastError());
        return false;
    }

    openGLContext = wglCreateContext(deviceContext);
    if (!openGLContext) {
        error = windowsError("wglCreateContext", "Could not create the OpenGL context.", GetLastError());
        return false;
    }
    if (!wglMakeCurrent(deviceContext, openGLContext)) {
        error = windowsError("wglMakeCurrent", "Could not activate the OpenGL context.", GetLastError());
        return false;
    }

    timerResolutionEnabled = timeBeginPeriod(1) == TIMERR_NOERROR;

    ShowWindow(window, SW_SHOW);
    // Начальный снимок запрашивается один раз; дальнейшие изменения приходят в оконную процедуру.
    RECT clientBounds = {};
    if (!GetClientRect(window, &clientBounds)) {
        error = windowsError("GetClientRect", "Could not read the initial client dimensions.", GetLastError());
        return false;
    }
    windowState = {static_cast<int>(clientBounds.right - clientBounds.left),
        static_cast<int>(clientBounds.bottom - clientBounds.top), GetFocus() == window, IsIconic(window) != FALSE,
        static_cast<float>(dpi) / 96};
    viewportNeedsUpdate = true;
    initialized = true;
    return processEvents(error);
}
}

void reportError(const Error& error, bool showDialog)
{
    std::string text = "OpenGLDemo: " + error.operation + ": " + error.message;
    if (error.nativeCode) {
        text += " (code " + std::to_string(*error.nativeCode) + ")";
    }
    std::cerr << text << '\n';
    if (showDialog) {
        // Строки Error хранятся в UTF-8, а MessageBoxW принимает UTF-16.
        const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (length > 0) {
            std::wstring wideText(static_cast<std::size_t>(length), L'\0');
            if (MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wideText.data(), length) != 0) {
                // Диалог не требует созданного окна или контекста OpenGL.
                if (MessageBoxW(nullptr, wideText.c_str(), L"OpenGLDemo - Error",
                    MB_OK | MB_ICONERROR | MB_TASKMODAL) == 0) {
                    const DWORD code = GetLastError();
                    std::cerr << "OpenGLDemo: MessageBoxW failed (code " << code << ").\n";
                }
            }
        }
    }
}

bool initialize(const WindowOptions& options, Error& error)
{
    error = {};
    if (initialized || windowClassRegistered || window || openGLContext) {
        error = Error{"initialize", "Call shutdown before initializing again.", std::nullopt};
        return false;
    }
    if (!validateWindowOptions(options, error)) {
        return false;
    }
    Detail::StartupRollback rollback;
    rollback.committed = initializeWindows(options, error);
    return rollback.committed;
}

bool getWindowState(WindowState& state, Error& error)
{
    if (!initialized) {
        error = {"getWindowState", "The platform is not initialized.", std::nullopt};
        return false;
    }
    state = windowState;
    return true;
}

bool setWindowTitle(const std::string& title, Error& error)
{
    if (!requireWindow("setWindowTitle", error) || !validateWindowTitle(title, "setWindowTitle", error)) {
        return false;
    }
    std::wstring wideTitle;
    if (!convertTitle(title, wideTitle, error)) {
        return false;
    }
    if (!SetWindowTextW(window, wideTitle.c_str())) {
        error = windowsError("SetWindowTextW", "Could not update the window title.", GetLastError());
        return false;
    }
    return true;
}

bool setVSync(bool enabled, Error& error)
{
    if (!requireWindow("setVSync", error)) {
        return false;
    }
    using GetExtensionsARB = const char* (WINAPI*)(HDC);
    using GetExtensionsEXT = const char* (WINAPI*)();
    using SwapInterval = BOOL (WINAPI*)(int);
    const auto getExtensionsARB = loadWglFunction<GetExtensionsARB>("wglGetExtensionsStringARB");
    const char* extensions = getExtensionsARB ? getExtensionsARB(deviceContext) : nullptr;
    if (!extensions) {
        const auto getExtensionsEXT = loadWglFunction<GetExtensionsEXT>("wglGetExtensionsStringEXT");
        extensions = getExtensionsEXT ? getExtensionsEXT() : nullptr;
    }
    const auto swapInterval = loadWglFunction<SwapInterval>("wglSwapIntervalEXT");
    if (!Detail::hasExtension(extensions, "WGL_EXT_swap_control") || !swapInterval) {
        error = {"setVSync", "VSync control is unavailable: WGL_EXT_swap_control is not supported.", std::nullopt};
        return false;
    }
    if (!swapInterval(enabled ? 1 : 0)) {
        error = windowsError("wglSwapIntervalEXT", "Could not change VSync.", GetLastError());
        return false;
    }
    return true;
}

bool processEvents(Error& error)
{
    if (initialized && !window && closeRequested) {
        Detail::finishInputFrame();
        return takeInputError(error);
    }
    if (!requireWindow("processEvents", error)) {
        return false;
    }
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            if (!closeRequested) {
                Detail::queueEvent(Engine::EventType::CloseRequested);
            }
            closeRequested = true;
            continue;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    // Область вывода занимает клиентскую область; матрицы игрового мира не меняются.
    if (window && viewportNeedsUpdate) {
        glViewport(0, 0, windowState.width, windowState.height);
        viewportNeedsUpdate = false;
    }
    Detail::finishInputFrame();
    return takeInputError(error);
}

bool isCloseRequested()
{
    return initialized && closeRequested;
}

bool present(Error& error)
{
    if (!requireWindow("present", error)) {
        return false;
    }
    if (windowState.minimized) {
        return true;
    }
    if (!SwapBuffers(deviceContext)) {
        error = windowsError("SwapBuffers", "Could not present the frame.", GetLastError());
        return false;
    }
    return true;
}

bool shutdown(Error& result)
{
    result = {};
    Error inputError;
    if (!takeInputError(inputError)) {
        Detail::keepFirstError(result, std::move(inputError));
    }
    if (timerResolutionEnabled) {
        // Multimedia API возвращает собственный код, а не GetLastError.
        const MMRESULT timerResult = timeEndPeriod(1);
        if (timerResult != TIMERR_NOERROR) {
            Detail::keepFirstError(result, Error{"timeEndPeriod", "Could not restore the timer resolution.", static_cast<int>(timerResult)});
        }
        timerResolutionEnabled = false;
    }
    if (openGLContext) {
        // После отказа активации контекст может не быть текущим.
        if (wglGetCurrentContext() == openGLContext && !wglMakeCurrent(nullptr, nullptr)) {
            Detail::keepFirstError(result, windowsError("wglMakeCurrent (detach)", "Could not detach the OpenGL context.", GetLastError()));
        }
        if (!wglDeleteContext(openGLContext)) {
            Detail::keepFirstError(result, windowsError("wglDeleteContext", "Could not delete the OpenGL context.", GetLastError()));
        }
        openGLContext = nullptr;
    }
    if (deviceContext && window) {
        // ReleaseDC не предоставляет расширенную ошибку через GetLastError.
        if (!ReleaseDC(window, deviceContext)) {
            Detail::keepFirstError(result, windowsError("ReleaseDC", "Could not release the window drawing surface."));
        }
        deviceContext = nullptr;
    }
    if (window) {
        const BOOL destroyed = DestroyWindow(window);
        const DWORD code = destroyed ? ERROR_SUCCESS : GetLastError();
        if (!takeInputError(inputError)) {
            Detail::keepFirstError(result, std::move(inputError));
        }
        if (!destroyed) {
            Detail::keepFirstError(result, windowsError("DestroyWindow", "Could not destroy the application window.", code));
        }
        window = nullptr;
    }
    if (windowClassRegistered) {
        if (!UnregisterClassW(WindowClassName, applicationInstance)) {
            Detail::keepFirstError(result, windowsError("UnregisterClass", "Could not unregister the window class.", GetLastError()));
        }
        windowClassRegistered = false;
    }
    if (previousDpiContext) {
        if (!setDpiContext(previousDpiContext)) {
            Detail::keepFirstError(result, windowsError("SetThreadDpiAwarenessContext", "Could not restore the thread DPI context.", GetLastError()));
        }
        previousDpiContext = nullptr;
    }
    applicationInstance = nullptr;
    closeRequested = false;
    initialized = false;
    windowState = {};
    viewportNeedsUpdate = false;
    heldKeys = {};
    heldMouseButtons = 0;
    mouseInside = false;
    highSurrogate = 0;
    Detail::resetInput();
    windowOptions = {};
    minimumTrackWidth = 0;
    minimumTrackHeight = 0;
    return result.operation.empty();
}
}
