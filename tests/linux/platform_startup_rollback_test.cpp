#include "platform.h"
#include "../common/platform_test_diagnostics.h"

#include <GL/glx.h>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <iostream>
#include <string>

namespace
{
bool rejectActivation = false;
bool rejectFilterMask = false;
unsigned int destroyedContexts = 0;
unsigned int closedMethods = 0;
unsigned int mapRequests = 0;
}

extern "C" int __real_XMapWindow(Display*, Window);
extern "C" int __wrap_XMapWindow(Display* display, Window window)
{
    ++mapRequests;
    return __real_XMapWindow(display, window);
}

extern "C" char* __real_XGetICValues(XIC, ...);
extern "C" char* __wrap_XGetICValues(XIC context, ...)
{
    // Платформа запрашивает только XNFilterEvents с выходным параметром long*.
    va_list arguments;
    va_start(arguments, context);
    const char* name = va_arg(arguments, const char*);
    long* mask = va_arg(arguments, long*);
    va_end(arguments);
    if (rejectFilterMask && std::strcmp(name, XNFilterEvents) == 0) {
        rejectFilterMask = false;
        static char failedName[] = XNFilterEvents;
        return failedName;
    }
    return __real_XGetICValues(context, name, mask, nullptr);
}

extern "C" void __real_XDestroyIC(XIC);
extern "C" void __wrap_XDestroyIC(XIC context)
{
    ++destroyedContexts;
    __real_XDestroyIC(context);
}

extern "C" Status __real_XCloseIM(XIM);
extern "C" Status __wrap_XCloseIM(XIM method)
{
    ++closedMethods;
    return __real_XCloseIM(method);
}

// Подмена позднего отказа запуска через --wrap; окно и контекст создаются реально.
extern "C" Bool __real_glXMakeCurrent(Display* display, GLXDrawable drawable, GLXContext context);

extern "C" Bool __wrap_glXMakeCurrent(Display* display, GLXDrawable drawable, GLXContext context)
{
    if (context && rejectActivation) {
        rejectActivation = false;
        return False;
    }
    return __real_glXMakeCurrent(display, drawable, context);
}

int main()
{
    const char* display = std::getenv("DISPLAY");
    if (!display || !*display) {
        std::cout << "Rollback tests require an X11 display.\n";
        return 77;
    }
    Platform::WindowOptions options;
    options.width = 320;
    options.height = 240;
    options.title = "Startup rollback test";
    Platform::Error failure;
    rejectFilterMask = true;
    const bool filterStarted = Platform::initialize(options, failure);
    Platform::WindowState filterState;
    Platform::Error filterError;
    if (filterStarted || rejectFilterMask || failure.operation.empty()
        || failure.operation != "XGetICValues"
        || failure.message.find(XNFilterEvents) == std::string::npos
        || destroyedContexts != 1 || closedMethods != 1 || mapRequests != 0
        || Platform::getWindowState(filterState, filterError) || glXGetCurrentContext()) {
        std::cerr << "XGetICValues failure must release XIC/XIM and report the failed attribute.\n";
        PlatformTest::reportError(failure);
        Platform::shutdown(failure);
        return 1;
    }
    if (!Platform::initialize(options, failure)) {
        std::cerr << "Restart after XGetICValues failure failed.\n";
        PlatformTest::reportError(failure);
        Platform::shutdown(failure);
        return 1;
    }
    Platform::shutdown(failure);
    if (!failure.operation.empty()) {
        PlatformTest::reportError(failure);
        return 1;
    }
    {
        const unsigned int beforeFailure = mapRequests;
        rejectActivation = true;
        const bool started = Platform::initialize(options, failure);
        if (started || rejectActivation || failure.operation != "glXMakeCurrent"
            || failure.message.empty()) {
            std::cerr << "Startup must retain the original error followed by cleanup errors.\n";
            PlatformTest::reportError(failure);
            Platform::shutdown(failure);
            return 1;
        }
        Platform::WindowState state;
        Platform::Error error;
        if (Platform::getWindowState(state, error) || Platform::isCloseRequested()
            || glXGetCurrentContext() != nullptr) {
            std::cerr << "Failed initialization must leave no usable platform state.\n";
            PlatformTest::reportError(failure);
            Platform::shutdown(failure);
            return 1;
        }
        if (mapRequests != beforeFailure) {
            std::cerr << "A failed context activation must not map the window.\n";
            PlatformTest::reportError(failure);
            Platform::shutdown(failure);
            return 1;
        }
        // Повторный запуск без shutdown после отказа инициализации.
        const bool restarted = Platform::initialize(options, failure);
        const bool stateRead = restarted && Platform::getWindowState(state, error);
        if (restarted && !stateRead) {
            Platform::reportError(error, false);
        }
        if (!restarted || !failure.operation.empty() || !stateRead || state.width != 320 || state.height != 240
            || mapRequests != beforeFailure + 1) {
            std::cerr << "Initialization after automatic rollback must succeed and clear old errors.\n";
            PlatformTest::reportError(failure);
            Platform::shutdown(failure);
            return 1;
        }
        Platform::shutdown(failure);
        if (!failure.operation.empty()) {
            std::cerr << "Shutdown after a successful retry must succeed.\n";
            PlatformTest::reportError(failure);
            return 1;
        }
    }
    return 0;
}
