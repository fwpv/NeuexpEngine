#include <windows.h>

namespace InputErrorTest
{
BOOL trackMouseEvent(LPTRACKMOUSEEVENT tracking);
BOOL screenToClient(HWND window, LPPOINT position);
BOOL releaseCapture();
}

// Подмены действуют только при компиляции этой тестовой копии платформы.
#define TrackMouseEvent InputErrorTest::trackMouseEvent
#define ScreenToClient InputErrorTest::screenToClient
#define ReleaseCapture InputErrorTest::releaseCapture
#include "../../platform/platform_windows.cpp"
#undef TrackMouseEvent
#undef ScreenToClient
#undef ReleaseCapture

#include "../common/platform_test_diagnostics.h"

#include <algorithm>
#include <array>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace
{
bool rejectTracking = false;
bool rejectConversion = false;
bool rejectRelease = false;
constexpr DWORD TrackingFailureCode = ERROR_NOT_ENOUGH_MEMORY;
constexpr DWORD ReleaseFailureCode = ERROR_ACCESS_DENIED;

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool hasEvent(Engine::EventType type)
{
    const auto& events = Platform::getEvents();
    return std::any_of(events.begin(), events.end(), [type](const auto& event) { return event.type == type; });
}

void checkFailure(const char* operation, std::optional<int> nativeCode)
{
    Platform::Error error;
    require(!Platform::processEvents(error) && error.operation == operation
        && error.nativeCode == nativeCode && !error.message.empty(),
        "Input failure lost its operation or native code.");
}
}

namespace InputErrorTest
{
BOOL trackMouseEvent(LPTRACKMOUSEEVENT tracking)
{
    if (rejectTracking) {
        rejectTracking = false;
        SetLastError(TrackingFailureCode);
        return FALSE;
    }
    return TrackMouseEvent(tracking);
}

BOOL screenToClient(HWND window, LPPOINT position)
{
    if (rejectConversion) {
        rejectConversion = false;
        // ScreenToClient не документирует расширенный код ошибки.
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    return ScreenToClient(window, position);
}

BOOL releaseCapture()
{
    if (rejectRelease) {
        rejectRelease = false;
        SetLastError(ReleaseFailureCode);
        return FALSE;
    }
    return ReleaseCapture();
}
}

int main()
{
    std::vector<Platform::Error> errors;
    try {
        Platform::WindowOptions options;
        options.width = 320;
        options.height = 240;
        options.title = "Input error handling test";
        require(Platform::initialize(options, errors), "Initialization failed.");
        HWND handle = WindowFromDC(wglGetCurrentDC());
        require(handle != nullptr, "The platform window is unavailable.");

        rejectTracking = true;
        SendMessageW(handle, WM_MOUSEMOVE, 0, MAKELPARAM(3, 4));
        rejectConversion = true;
        SendMessageW(handle, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), MAKELPARAM(30, 40));
        checkFailure("TrackMouseEvent", static_cast<int>(TrackingFailureCode));
        require(!rejectTracking && !rejectConversion && hasEvent(Engine::EventType::MouseMoved)
            && !hasEvent(Engine::EventType::MouseWheel),
            "A later failure replaced the first error or published invalid wheel coordinates.");
        Platform::Error error;
        require(Platform::processEvents(error), "A reported error leaked into the next frame.");

        rejectConversion = true;
        SendMessageW(handle, WM_MOUSEHWHEEL, MAKEWPARAM(0, WHEEL_DELTA), MAKELPARAM(30, 40));
        checkFailure("ScreenToClient", std::nullopt);
        require(!rejectConversion && !hasEvent(Engine::EventType::MouseWheel),
            "Failed coordinate conversion published a wheel event.");

        for (const UINT message : std::array<UINT, 3>{ WM_LBUTTONUP, WM_CANCELMODE, WM_KILLFOCUS }) {
            SendMessageW(handle, WM_SETFOCUS, 0, 0);
            SendMessageW(handle, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(3, 4));
            require(GetCapture() == handle, "Mouse capture was not acquired.");
            rejectRelease = true;
            SendMessageW(handle, message, 0, MAKELPARAM(3, 4));
            checkFailure("ReleaseCapture", static_cast<int>(ReleaseFailureCode));
            require(!rejectRelease && !Platform::getInput().button(Engine::MouseButton::Left).down,
                "Capture release failure left the button held in the input snapshot.");
            if (GetCapture() == handle) {
                require(ReleaseCapture() != FALSE, "Could not release the test capture.");
            }
            require(Platform::processEvents(error), "Capture release error prevented recovery.");
        }

        rejectTracking = true;
        SendMessageW(handle, WM_MOUSEMOVE, 0, MAKELPARAM(3, 4));
        require(DestroyWindow(handle) != FALSE, "External window destruction failed.");
        checkFailure("TrackMouseEvent", static_cast<int>(TrackingFailureCode));
        require(Platform::isCloseRequested() && hasEvent(Engine::EventType::WindowDestroyed),
            "Window destruction was lost when publishing an input error.");
        require(Platform::processEvents(error), "Destroyed window retained a reported error.");
        Platform::shutdown(errors);
        require(errors.empty(), "Cleanup after external destruction failed.");

        require(Platform::initialize(options, errors), "Restart after input errors failed.");
        handle = WindowFromDC(wglGetCurrentDC());
        rejectConversion = true;
        SendMessageW(handle, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), MAKELPARAM(30, 40));
        Platform::shutdown(errors);
        require(errors.size() == 1 && errors.front().operation == "ScreenToClient"
            && !errors.front().nativeCode && !rejectConversion,
            "Shutdown lost an input error received before processEvents.");
        Platform::shutdown(errors);
        require(errors.empty(), "Repeated cleanup retained the input error.");
        require(Platform::initialize(options, errors) && errors.empty(), "Final restart failed.");
        require(Platform::processEvents(error), "Restart retained an input error.");
        Platform::shutdown(errors);
        require(errors.empty(), "Final cleanup failed.");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        PlatformTest::shutdownAfterFailure(errors);
        return 1;
    }
    return 0;
}
