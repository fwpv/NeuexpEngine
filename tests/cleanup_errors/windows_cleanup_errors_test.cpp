#include "platform.h"

#include <windows.h>
#include <mmsystem.h>
#include <array>
#include <cstddef>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>

#undef wglMakeCurrent
#undef wglDeleteContext
#undef ReleaseDC
#undef DestroyWindow
#undef UnregisterClassW
#undef GetProcAddress
#undef timeEndPeriod

namespace
{
enum class Failure
{
    NoFailure,
    Detach,
    DeleteContext,
    ReleaseSurface,
    DestroyWindow,
    UnregisterClass,
    RestoreDpi,
    EndTimer,
    Count
};

Failure failure = Failure::NoFailure;
std::array<int, static_cast<std::size_t>(Failure::Count)> calls = {};

bool fails(Failure operation)
{
    ++calls[static_cast<std::size_t>(operation)];
    if (failure == operation) {
        SetLastError(ERROR_ACCESS_DENIED);
        return true;
    }
    return false;
}

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

HANDLE WINAPI restoreDpi(HANDLE context)
{
    if (fails(Failure::RestoreDpi)) {
        return nullptr;
    }
    using SetDpiContext = HANDLE (WINAPI*)(HANDLE);
    const auto native = reinterpret_cast<SetDpiContext>(reinterpret_cast<void (WINAPI*)()>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetThreadDpiAwarenessContext")));
    return native ? native(context) : nullptr;
}

void verifyRetry(Failure selected, const char* operation)
{
    Platform::WindowOptions options;
    options.title = "Engine cleanup retry test";
    Platform::Error error;
    require(Platform::initialize(options, error), "Cleanup test initialization failed.");
    const HWND window = FindWindowW(nullptr, L"Engine cleanup retry test");
    require(window != nullptr, "Could not locate the cleanup test window.");
    calls = {};
    failure = selected;
    require(!Platform::shutdown(error), "Cleanup failure was reported as success.");
    require(error.operation == operation && !error.message.empty(), "Cleanup failure lost its diagnostic.");
    if (selected != Failure::ReleaseSurface) {
        const int expectedCode = selected == Failure::EndTimer ? TIMERR_NOCANDO : ERROR_ACCESS_DENIED;
        require(error.nativeCode == expectedCode, "Cleanup failure lost its native error code.");
    }
    const auto index = static_cast<std::size_t>(selected);
    require(calls[index] == 1, "Cleanup did not attempt the selected operation.");
    if (selected == Failure::Detach || selected == Failure::DeleteContext) {
        require(calls[static_cast<std::size_t>(Failure::ReleaseSurface)] == 0,
            "Cleanup released the surface while its GL context was still owned.");
    }
    if (selected == Failure::Detach) {
        require(calls[static_cast<std::size_t>(Failure::DeleteContext)] == 0,
            "Cleanup deleted a context that could not be detached.");
    }
    if (selected == Failure::Detach || selected == Failure::DeleteContext || selected == Failure::ReleaseSurface) {
        require(calls[static_cast<std::size_t>(Failure::DestroyWindow)] == 0 && IsWindow(window),
            "Cleanup destroyed a window while its dependent resources were still owned.");
    }
    if (selected == Failure::DestroyWindow) {
        require(calls[static_cast<std::size_t>(Failure::UnregisterClass)] == 0 && IsWindow(window),
            "Cleanup lost the window or attempted to unregister its live class.");
    }
    require(!Platform::initialize(options, error) && error.operation == "initialize",
        "Initialization overwrote resources awaiting cleanup.");
    require(!Platform::shutdown(error) && error.operation == operation && calls[index] == 2,
        "Repeated cleanup forgot the failed operation.");
    failure = Failure::NoFailure;
    require(Platform::shutdown(error) && error.operation.empty() && calls[index] == 3,
        "Cleanup did not recover after the native failure was removed.");
    require(!IsWindow(window) && wglGetCurrentContext() == nullptr,
        "Successful retry retained a window or current context.");
    require(Platform::shutdown(error), "Cleanup was not idempotent after recovery.");
    require(Platform::initialize(options, error), "Initialization did not recover after cleanup.");
    require(Platform::shutdown(error), "Cleanup after reinitialization failed.");
}
}

namespace Platform::Testing
{
BOOL WINAPI detachContext(HDC surface, HGLRC context)
{
    return !context && fails(Failure::Detach) ? FALSE : wglMakeCurrent(surface, context);
}

BOOL WINAPI deleteContext(HGLRC context)
{
    return fails(Failure::DeleteContext) ? FALSE : wglDeleteContext(context);
}

int WINAPI releaseSurface(HWND window, HDC surface)
{
    return fails(Failure::ReleaseSurface) ? 0 : ReleaseDC(window, surface);
}

BOOL WINAPI destroyWindow(HWND window)
{
    return fails(Failure::DestroyWindow) ? FALSE : DestroyWindow(window);
}

BOOL WINAPI unregisterWindowClass(LPCWSTR name, HINSTANCE instance)
{
    return fails(Failure::UnregisterClass) ? FALSE : UnregisterClassW(name, instance);
}

FARPROC WINAPI lookupCleanupFunction(HMODULE module, LPCSTR name)
{
    if (std::strcmp(name, "SetThreadDpiAwarenessContext") == 0) {
        return reinterpret_cast<FARPROC>(reinterpret_cast<void (WINAPI*)()>(restoreDpi));
    }
    return GetProcAddress(module, name);
}

MMRESULT WINAPI endTimerPeriod(UINT period)
{
    return fails(Failure::EndTimer) ? TIMERR_NOCANDO : timeEndPeriod(period);
}
}

int main()
{
    try {
        verifyRetry(Failure::Detach, "wglMakeCurrent (detach)");
        verifyRetry(Failure::DeleteContext, "wglDeleteContext");
        verifyRetry(Failure::ReleaseSurface, "ReleaseDC");
        verifyRetry(Failure::DestroyWindow, "DestroyWindow");
        verifyRetry(Failure::UnregisterClass, "UnregisterClass");
        verifyRetry(Failure::RestoreDpi, "SetThreadDpiAwarenessContext");
        verifyRetry(Failure::EndTimer, "timeEndPeriod");
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        failure = Failure::NoFailure;
        Platform::Error error;
        if (!Platform::shutdown(error)) {
            Platform::reportError(error, false);
        }
        return 1;
    }
}
