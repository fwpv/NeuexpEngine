#include "app/platform.h"
#include "app/input.h"

#include <windows.h>
#include <exception>
#include <iostream>
#include <stdexcept>

#undef SetCapture

namespace
{
bool failCapture = false;

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}
}

namespace Platform::Testing
{
HWND WINAPI captureMouse(HWND window)
{
    return failCapture ? nullptr : SetCapture(window);
}
}

int main()
{
    Platform::Error error;
    try {
        Platform::WindowOptions options;
        options.title = "Engine capture error test";
        require(Platform::initialize(options, error), "Initialization failed.");
        const HWND window = FindWindowW(nullptr, L"Engine capture error test");
        require(window != nullptr, "Could not find the test window.");
        SendMessageW(window, WM_SETFOCUS, 0, 0);
        require(Platform::processEvents(error), "Could not publish focus.");
        require(GetCapture() == nullptr, "Mouse was already captured before the test.");

        failCapture = true;
        SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(20, 30));
        require(!Platform::processEvents(error), "Capture failure was reported as success.");
        require(error.operation == "SetCapture" && !error.message.empty(), "Capture failure lost its diagnostic.");
        const auto failedButton = Platform::getInput().button(NeuexpEngine::MouseButton::Left);
        require(!failedButton.down && !failedButton.pressed, "Failed capture published a mouse press.");
        for (const auto& event : Platform::getEvents()) {
            require(event.type != NeuexpEngine::EventType::MousePressed, "Failed capture published a mouse event.");
        }

        failCapture = false;
        SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(20, 30));
        require(Platform::processEvents(error), "Event processing did not recover after capture failure.");
        require(GetCapture() == window && Platform::getInput().button(NeuexpEngine::MouseButton::Left).down,
            "Successful capture did not start dragging.");
        require(ReleaseCapture() != FALSE, "Could not release the test capture.");
        require(Platform::processEvents(error), "Could not publish capture loss.");
        const auto releasedButton = Platform::getInput().button(NeuexpEngine::MouseButton::Left);
        require(!releasedButton.down && releasedButton.released, "Capture loss left the mouse button held.");
        require(Platform::shutdown(error), "Shutdown failed.");
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        Platform::reportError(error, false);
        Platform::shutdown(error);
        return 1;
    }
}
