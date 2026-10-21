#include "platform.h"
#include "../common/platform_test_diagnostics.h"

#include <GL/glx.h>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <iostream>
#include <vector>

namespace
{
bool rejectActivation = false;
bool rejectDetach = false;
bool rejectFilterMask = false;
unsigned int destroyedContexts = 0;
unsigned int closedMethods = 0;
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
    if (!context && rejectDetach) {
        rejectDetach = false;
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
    std::vector<Platform::Error> errors;
    rejectFilterMask = true;
    const bool filterStarted = Platform::initialize(options, errors);
    Platform::WindowState filterState;
    Platform::Error filterError;
    if (filterStarted || rejectFilterMask || errors.size() != 1
        || errors.front().operation != "XGetICValues"
        || errors.front().message.find(XNFilterEvents) == std::string::npos
        || destroyedContexts != 1 || closedMethods != 1
        || Platform::getWindowState(filterState, filterError) || glXGetCurrentContext()) {
        std::cerr << "XGetICValues failure must release XIC/XIM and report the failed attribute.\n";
        PlatformTest::shutdownAfterFailure(errors);
        return 1;
    }
    if (!Platform::initialize(options, errors)) {
        std::cerr << "Restart after XGetICValues failure failed.\n";
        PlatformTest::shutdownAfterFailure(errors);
        return 1;
    }
    Platform::shutdown(errors);
    if (!errors.empty()) {
        PlatformTest::reportErrors(errors);
        return 1;
    }
    for (const bool cleanupFailure : { false, true }) {
        rejectActivation = true;
        rejectDetach = cleanupFailure;
        const bool started = Platform::initialize(options, errors);
        if (started || errors.size() != (cleanupFailure ? 2U : 1U)
            || errors.front().operation != "glXMakeCurrent"
            || (cleanupFailure && errors[1].operation != "glXMakeCurrent (detach)")) {
            std::cerr << "Startup must retain the original error followed by cleanup errors.\n";
            PlatformTest::shutdownAfterFailure(errors);
            return 1;
        }
        Platform::WindowState state;
        Platform::Error error;
        if (Platform::getWindowState(state, error) || Platform::isCloseRequested()
            || glXGetCurrentContext() != nullptr) {
            std::cerr << "Failed initialization must leave no usable platform state.\n";
            PlatformTest::shutdownAfterFailure(errors);
            return 1;
        }
        // Повторный запуск без shutdown после отказа инициализации.
        const bool restarted = Platform::initialize(options, errors);
        const bool stateRead = restarted && Platform::getWindowState(state, error);
        if (restarted && !stateRead) {
            Platform::reportError(error, false);
        }
        if (!restarted || !errors.empty() || !stateRead || state.width != 320 || state.height != 240) {
            std::cerr << "Initialization after automatic rollback must succeed and clear old errors.\n";
            PlatformTest::shutdownAfterFailure(errors);
            return 1;
        }
        Platform::shutdown(errors);
        if (!errors.empty()) {
            std::cerr << "Shutdown after a successful retry must succeed.\n";
            PlatformTest::reportErrors(errors);
            return 1;
        }
    }
    return 0;
}
