#include "platform.h"
#include "../common/platform_test_diagnostics.h"

#include <GL/glx.h>
#include <X11/XKBlib.h>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace
{
Display* platformDisplay = nullptr;
bool rejectQuery = false;
bool rejectSelection = false;
bool rejectKeyboard = false;
bool rejectActivation = false;
bool rejectClose = false;
bool failContextCleanup = false;
bool failMethodCleanup = false;

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void checkError(const std::vector<Platform::Error>& errors, std::size_t index,
    const char* operation, std::optional<int> nativeCode)
{
    require(index < errors.size() && errors[index].operation == operation
        && errors[index].nativeCode == nativeCode && !errors[index].message.empty(),
        "An error lost its operation, native code or position.");
}
}

extern "C" Display* __real_XOpenDisplay(const char*);
extern "C" Display* __wrap_XOpenDisplay(const char* name)
{
    platformDisplay = __real_XOpenDisplay(name);
    return platformDisplay;
}

extern "C" Bool __real_XkbQueryExtension(Display*, int*, int*, int*, int*, int*);
extern "C" Bool __wrap_XkbQueryExtension(Display* display, int* opcode, int* eventBase,
    int* errorBase, int* major, int* minor)
{
    if (rejectQuery) {
        rejectQuery = false;
        return False;
    }
    return __real_XkbQueryExtension(display, opcode, eventBase, errorBase, major, minor);
}

extern "C" Bool __real_XkbSelectEventDetails(Display*, unsigned int, unsigned int,
    unsigned long, unsigned long);
extern "C" Bool __wrap_XkbSelectEventDetails(Display* display, unsigned int device,
    unsigned int event, unsigned long changed, unsigned long values)
{
    if (rejectSelection) {
        rejectSelection = false;
        // Настоящая отложенная ошибка сервера сопровождает отказ вызова.
        XDestroyWindow(display, None);
        return False;
    }
    return __real_XkbSelectEventDetails(display, device, event, changed, values);
}

extern "C" XkbDescPtr __real_XkbGetKeyboard(Display*, unsigned int, unsigned int);
extern "C" XkbDescPtr __wrap_XkbGetKeyboard(Display* display, unsigned int components,
    unsigned int device)
{
    if (rejectKeyboard) {
        rejectKeyboard = false;
        // Неверный идентификатор устройства отклоняет сам X-сервер.
        return __real_XkbGetKeyboard(display, components, 0xfffe);
    }
    return __real_XkbGetKeyboard(display, components, device);
}

extern "C" Bool __real_glXMakeCurrent(Display*, GLXDrawable, GLXContext);
extern "C" Bool __wrap_glXMakeCurrent(Display* display, GLXDrawable drawable, GLXContext context)
{
    if (context && rejectActivation) {
        rejectActivation = false;
        return False;
    }
    return __real_glXMakeCurrent(display, drawable, context);
}

extern "C" void __real_XDestroyIC(XIC);
extern "C" void __wrap_XDestroyIC(XIC context)
{
    __real_XDestroyIC(context);
    if (failContextCleanup) {
        failContextCleanup = false;
        XFreeColormap(platformDisplay, None);
    }
}

extern "C" Status __real_XCloseIM(XIM);
extern "C" Status __wrap_XCloseIM(XIM method)
{
    const Status result = __real_XCloseIM(method);
    if (failMethodCleanup) {
        failMethodCleanup = false;
        XDestroyWindow(platformDisplay, None);
    }
    if (rejectClose) {
        rejectClose = false;
        return False;
    }
    return result;
}

int main()
{
    const char* display = std::getenv("DISPLAY");
    if (!display || !*display) {
        return 77;
    }
    std::vector<Platform::Error> errors;
    try {
        Platform::WindowOptions options;
        options.width = 320;
        options.height = 240;
        options.title = "Platform error handling test";

        rejectQuery = true;
        require(!Platform::initialize(options, errors), "Missing XKB must reject startup.");
        require(errors.size() == 1 && !rejectQuery, "XKB query failure was not isolated.");
        checkError(errors, 0, "XkbQueryExtension", std::nullopt);

        rejectSelection = true;
        require(!Platform::initialize(options, errors), "Failed subscription must reject startup.");
        require(errors.size() == 1 && !rejectSelection, "Subscription error leaked into cleanup.");
        checkError(errors, 0, "XkbSelectEventDetails", BadWindow);

        rejectKeyboard = true;
        require(!Platform::initialize(options, errors), "Failed keyboard read must reject startup.");
        require(errors.size() == 1 && !rejectKeyboard && errors.front().nativeCode.has_value(),
            "Keyboard protocol error was reported as a cleanup error.");
        checkError(errors, 0, "XkbGetKeyboard", errors.front().nativeCode);

        require(Platform::initialize(options, errors), "Restart after XKB failures failed.");
        rejectClose = true;
        Platform::shutdown(errors);
        require(errors.size() == 1 && !rejectClose, "XCloseIM failure was ignored.");
        checkError(errors, 0, "XCloseIM", std::nullopt);

        for (const bool pendingFailure : { false, true }) {
            require(Platform::initialize(options, errors), "Restart after cleanup failure failed.");
            if (pendingFailure) {
                XDestroyWindow(platformDisplay, None);
            }
            failContextCleanup = true;
            failMethodCleanup = true;
            rejectClose = true;
            Platform::shutdown(errors);
            const std::size_t offset = pendingFailure ? 1U : 0U;
            require(errors.size() == offset + 2 && !failContextCleanup
                && !failMethodCleanup && !rejectClose, "Cleanup errors were lost or duplicated.");
            if (pendingFailure) {
                checkError(errors, 0, "pending X11 requests before shutdown", BadWindow);
            }
            checkError(errors, offset, "XDestroyIC", BadColor);
            checkError(errors, offset + 1, "XCloseIM", BadWindow);
            require(!glXGetCurrentContext(), "Cleanup left an active context.");
        }

        rejectActivation = true;
        failContextCleanup = true;
        failMethodCleanup = true;
        require(!Platform::initialize(options, errors), "Context activation must fail.");
        require(errors.size() == 3 && !rejectActivation, "Rollback changed the error order.");
        checkError(errors, 0, "glXMakeCurrent", std::nullopt);
        checkError(errors, 1, "XDestroyIC", BadColor);
        checkError(errors, 2, "XCloseIM", BadWindow);

        require(Platform::initialize(options, errors) && errors.empty(), "Final restart failed.");
        Platform::shutdown(errors);
        require(errors.empty(), "Successful cleanup retained old errors.");
        Platform::shutdown(errors);
        require(errors.empty(), "Repeated cleanup retained old errors.");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        PlatformTest::shutdownAfterFailure(errors);
        return 1;
    }
    return 0;
}
