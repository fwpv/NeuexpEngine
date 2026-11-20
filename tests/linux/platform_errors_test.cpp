#include "app/platform.h"

#include <GL/glx.h>
#include <X11/XKBlib.h>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <sstream>
#include <string>
#include <streambuf>

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

void checkError(const Platform::Error& error, const char* operation, std::optional<int> nativeCode)
{
    require(error.operation == operation && error.nativeCode == nativeCode && !error.message.empty(),
        "An error lost its operation, native code or message.");
}

class ErrorLog
{
public:
    ErrorLog()
        : previous(std::cerr.rdbuf(output.rdbuf()))
    {
    }

    ~ErrorLog()
    {
        std::cerr.rdbuf(previous);
    }

    std::string text() const
    {
        return output.str();
    }

private:
    std::ostringstream output;
    std::streambuf* previous;
};
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
    Platform::Error error;
    try {
        Platform::WindowOptions options;
        options.width = 320;
        options.height = 240;
        options.title = "Platform error handling test";

        rejectQuery = true;
        require(!Platform::initialize(options, error), "Missing XKB must reject startup.");
        require(!rejectQuery, "XKB query failure was not isolated.");
        checkError(error, "XkbQueryExtension", std::nullopt);

        rejectSelection = true;
        require(!Platform::initialize(options, error), "Failed subscription must reject startup.");
        require(!rejectSelection, "Subscription error leaked into cleanup.");
        checkError(error, "XkbSelectEventDetails", BadWindow);

        rejectKeyboard = true;
        require(!Platform::initialize(options, error), "Failed keyboard read must reject startup.");
        require(!rejectKeyboard && error.nativeCode.has_value(),
            "Keyboard protocol error was reported as a cleanup error.");
        checkError(error, "XkbGetKeyboard", error.nativeCode);

        require(Platform::initialize(options, error), "Restart after XKB failures failed.");
        rejectClose = true;
        Platform::shutdown(error);
        require(!rejectClose, "XCloseIM failure was ignored.");
        checkError(error, "XCloseIM", std::nullopt);

        for (const bool pendingFailure : {false, true}) {
            require(Platform::initialize(options, error), "Restart after cleanup failure failed.");
            if (pendingFailure) {
                XDestroyWindow(platformDisplay, None);
            }
            failContextCleanup = true;
            failMethodCleanup = true;
            rejectClose = true;
            ErrorLog log;
            require(!Platform::shutdown(error), "Cleanup failure was reported as success.");
            require(!failContextCleanup && !failMethodCleanup && !rejectClose,
                "Cleanup stopped before releasing all resources.");
            if (pendingFailure) {
                checkError(error, "pending X11 requests before shutdown", BadWindow);
                require(log.text().find("XDestroyIC") != std::string::npos,
                    "Secondary XIC cleanup diagnostic was lost.");
            } else {
                checkError(error, "XDestroyIC", BadColor);
            }
            require(log.text().find("XCloseIM") != std::string::npos,
                "Secondary XIM cleanup diagnostic was lost.");
            require(!glXGetCurrentContext(), "Cleanup left an active context.");
        }

        {
            ErrorLog log;
            rejectActivation = true;
            failContextCleanup = true;
            failMethodCleanup = true;
            require(!Platform::initialize(options, error), "Context activation must fail.");
            require(!rejectActivation && !failContextCleanup && !failMethodCleanup,
                "Rollback skipped cleanup.");
            checkError(error, "glXMakeCurrent", std::nullopt);
            const auto contextPosition = log.text().find("XDestroyIC");
            const auto methodPosition = log.text().find("XCloseIM");
            require(contextPosition != std::string::npos && methodPosition != std::string::npos,
                "Rollback lost secondary diagnostics.");
        }

        require(Platform::initialize(options, error) && error.operation.empty(), "Final restart failed.");
        Platform::shutdown(error);
        require(error.operation.empty(), "Successful cleanup retained old errors.");
        Platform::shutdown(error);
        require(error.operation.empty(), "Repeated cleanup retained old errors.");
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        Platform::reportError(error, false);
        if (!Platform::shutdown(error)) {
            Platform::reportError(error, false);
        }
        return 1;
    }
    return 0;
}
