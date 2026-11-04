#include "platform.h"
#include "../common/platform_test_diagnostics.h"

#include <GL/glx.h>
#include <X11/Xlib.h>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
Display* externalDisplay = nullptr;
Window targetWindow = None;
bool destroyDuringRequest = false;
bool injectUnrelatedError = false;
}

extern "C" int __real_XDestroyWindow(Display* display, Window window);

// Второе соединение удаляет настоящее окно непосредственно перед запросом движка.
extern "C" int __wrap_XDestroyWindow(Display* display, Window window)
{
    if (display != externalDisplay && window == targetWindow) {
        if (destroyDuringRequest) {
            destroyDuringRequest = false;
            __real_XDestroyWindow(externalDisplay, window);
            XSync(externalDisplay, False);
        }
        if (injectUnrelatedError) {
            injectUnrelatedError = false;
            // Ошибка другого ресурса не должна скрываться за успешным удалением окна.
            __real_XDestroyWindow(display, None);
        }
    }
    return __real_XDestroyWindow(display, window);
}

int main()
{
    const char* displayName = std::getenv("DISPLAY");
    if (!displayName || !*displayName) {
        std::cout << "External destruction tests require an X11 display.\n";
        return 77;
    }
    externalDisplay = XOpenDisplay(nullptr);
    if (!externalDisplay) {
        std::cerr << "Could not open the external X11 connection.\n";
        return 1;
    }

    enum class Scenario { BeforeShutdown, DuringDestroyRequest, UnrelatedError };
    int exitCode = 0;
    for (const auto scenario : { Scenario::BeforeShutdown, Scenario::DuringDestroyRequest,
                                 Scenario::UnrelatedError }) {
        Platform::WindowOptions options;
        options.width = 320;
        options.height = 240;
        options.title = "External destruction test";
        Platform::Error failure;
        if (!Platform::initialize(options, failure)) {
            PlatformTest::reportError(failure);
            exitCode = 1;
            break;
        }
        targetWindow = glXGetCurrentDrawable();
        if (scenario == Scenario::BeforeShutdown) {
            __real_XDestroyWindow(externalDisplay, targetWindow);
            XSync(externalDisplay, False);
        }
        destroyDuringRequest = scenario == Scenario::DuringDestroyRequest;
        injectUnrelatedError = scenario == Scenario::UnrelatedError;

        // Не вызываем processEvents: shutdown должен учитывать ещё не разобранное DestroyNotify.
        Platform::shutdown(failure);
        const bool passed = scenario == Scenario::UnrelatedError
            ? !failure.operation.empty() && failure.operation == "XDestroyWindow"
                && failure.nativeCode == BadWindow
            : failure.operation.empty();
        if (!passed) {
            std::cerr << "Unexpected shutdown result in scenario " << static_cast<int>(scenario) << ".\n";
            PlatformTest::reportError(failure);
            exitCode = 1;
            break;
        }
        Platform::Error error;
        Platform::WindowState state;
        if (Platform::getWindowState(state, error) || Platform::isCloseRequested()
            || glXGetCurrentContext() != nullptr || destroyDuringRequest || injectUnrelatedError) {
            std::cerr << "Shutdown left usable state or did not execute the injected request.\n";
            exitCode = 1;
            break;
        }
    }
    XCloseDisplay(externalDisplay);
    externalDisplay = nullptr;
    return exitCode;
}
