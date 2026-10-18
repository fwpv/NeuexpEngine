#include "platform.h"

#include <GL/glx.h>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
bool rejectActivation = false;
bool rejectDetach = false;
}

// Подмена только в тестовом исполняемом файле через --wrap: окно и контекст создаются реально.
// Отказываем на позднем шаге запуска, чтобы проверить очистку уже выделенных ресурсов.
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
    for (const bool cleanupFailure : { false, true }) {
        rejectActivation = true;
        rejectDetach = cleanupFailure;
        const bool started = Platform::initialize(options, errors);
        if (started || errors.size() != (cleanupFailure ? 2U : 1U)
            || errors.front().operation != "glXMakeCurrent"
            || (cleanupFailure && errors[1].operation != "glXMakeCurrent (detach)")) {
            std::cerr << "Startup must retain the original error followed by cleanup errors.\n";
            Platform::shutdown(errors);
            return 1;
        }
        Platform::WindowState state;
        Platform::Error error;
        if (Platform::getWindowState(state, error) || Platform::isCloseRequested()
            || glXGetCurrentContext() != nullptr) {
            std::cerr << "Failed initialization must leave no usable platform state.\n";
            Platform::shutdown(errors);
            return 1;
        }
        // Без пользовательского shutdown после ошибки: следующий запуск должен быть успешным.
        if (!Platform::initialize(options, errors) || !errors.empty()
            || !Platform::getWindowState(state, error) || state.width != 320 || state.height != 240) {
            std::cerr << "Initialization after automatic rollback must succeed and clear old errors.\n";
            Platform::shutdown(errors);
            return 1;
        }
        Platform::shutdown(errors);
        if (!errors.empty()) {
            std::cerr << "Shutdown after a successful retry must succeed.\n";
            return 1;
        }
    }
    return 0;
}
