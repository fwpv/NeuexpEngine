#include "platform.h"
#include "../common/platform_test_diagnostics.h"
#include "windows_gl_context.h"

#include <iostream>
#include <vector>

namespace
{
bool rejectActivation = false;
bool rejectDetach = false;
HWND failedWindow = nullptr;
constexpr DWORD ActivationFailureCode = ERROR_INVALID_HANDLE;
constexpr DWORD DetachFailureCode = ERROR_GEN_FAILURE;
}

namespace Platform::Testing
{
// Эта реализация используется только в тестовой копии платформы.
// При обычном запуске и последующем успешном повторе вызывается настоящий wglMakeCurrent.
BOOL makeContextCurrent(HDC deviceContext, HGLRC context)
{
    if (context && rejectActivation) {
        rejectActivation = false;
        // WindowFromDC подтверждает, что отказ происходит после создания окна и его HDC.
        failedWindow = WindowFromDC(deviceContext);
        SetLastError(ActivationFailureCode);
        return FALSE;
    }
    if (!context && rejectDetach) {
        rejectDetach = false;
        SetLastError(DetachFailureCode);
        return FALSE;
    }
    return wglMakeCurrent(deviceContext, context);
}
}

int main()
{
    Platform::WindowOptions options;
    options.width = 320;
    options.height = 240;
    options.title = "Startup rollback test";
    std::vector<Platform::Error> errors;
    for (const bool cleanupFailure : { false, true }) {
        rejectActivation = true;
        rejectDetach = cleanupFailure;
        failedWindow = nullptr;
        const bool started = Platform::initialize(options, errors);
        if (started || errors.size() != (cleanupFailure ? 2U : 1U)
            || errors.front().operation != "wglMakeCurrent"
            || errors.front().nativeCode != static_cast<int>(ActivationFailureCode)
            || errors.front().message.empty()
            || (cleanupFailure && (errors[1].operation != "wglMakeCurrent (detach)"
                || errors[1].nativeCode != static_cast<int>(DetachFailureCode)
                || errors[1].message.empty()))) {
            std::cerr << "Startup must retain the original error followed by cleanup errors.\n";
            PlatformTest::shutdownAfterFailure(errors);
            return 1;
        }
        Platform::WindowState state;
        Platform::Error error;
        if (rejectActivation || rejectDetach || !failedWindow || IsWindow(failedWindow)
            || Platform::getWindowState(state, error) || Platform::isCloseRequested()
            || wglGetCurrentContext() != nullptr || wglGetCurrentDC() != nullptr) {
            std::cerr << "Failed initialization must destroy its window and leave no usable platform state.\n";
            PlatformTest::shutdownAfterFailure(errors);
            return 1;
        }
        // Без пользовательского shutdown после ошибки. Проверяем также повторную регистрацию класса окна.
        const bool restarted = Platform::initialize(options, errors);
        const bool stateRead = restarted && Platform::getWindowState(state, error);
        if (restarted && !stateRead) {
            Platform::reportError(error, false);
        }
        if (!restarted || !errors.empty() || !stateRead || state.width != 320 || state.height != 240
            || wglGetCurrentContext() == nullptr || wglGetCurrentDC() == nullptr) {
            std::cerr << "Initialization after automatic rollback must succeed and clear old errors.\n";
            PlatformTest::shutdownAfterFailure(errors);
            return 1;
        }
        const HWND retryWindow = WindowFromDC(wglGetCurrentDC());
        Platform::shutdown(errors);
        if (!errors.empty() || !retryWindow || IsWindow(retryWindow)
            || wglGetCurrentContext() != nullptr || wglGetCurrentDC() != nullptr) {
            std::cerr << "Shutdown after a successful retry must release the window and detach the context.\n";
            PlatformTest::reportErrors(errors);
            return 1;
        }
    }
    return 0;
}
