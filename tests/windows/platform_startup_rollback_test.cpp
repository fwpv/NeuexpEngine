#include "platform.h"
#include "../common/platform_test_diagnostics.h"
#include "windows_gl_context.h"

#include <iostream>
#include <vector>

namespace
{
bool rejectActivation = false;
bool rejectDeletion = false;
unsigned int detachCount = 0;
HWND failedWindow = nullptr;
constexpr DWORD ActivationFailureCode = ERROR_INVALID_HANDLE;
constexpr DWORD DeletionFailureCode = ERROR_GEN_FAILURE;
enum class PixelFailure { None, Read, Window, OpenGL, DoubleBuffer, Type, Color, Depth };
PixelFailure pixelFailure = PixelFailure::None;
}

namespace Platform::Testing
{
int describePixelFormat(HDC deviceContext, int format, UINT size, LPPIXELFORMATDESCRIPTOR description)
{
    const int result = DescribePixelFormat(deviceContext, format, size, description);
    if (pixelFailure == PixelFailure::None || result == 0) {
        return result;
    }
    failedWindow = WindowFromDC(deviceContext);
    const auto failure = pixelFailure;
    pixelFailure = PixelFailure::None;
    switch (failure) {
        case PixelFailure::Read: SetLastError(ERROR_GEN_FAILURE); return 0;
        case PixelFailure::Window: description->dwFlags &= ~static_cast<DWORD>(PFD_DRAW_TO_WINDOW); break;
        case PixelFailure::OpenGL: description->dwFlags &= ~static_cast<DWORD>(PFD_SUPPORT_OPENGL); break;
        case PixelFailure::DoubleBuffer: description->dwFlags &= ~static_cast<DWORD>(PFD_DOUBLEBUFFER); break;
        case PixelFailure::Type: description->iPixelType = PFD_TYPE_COLORINDEX; break;
        case PixelFailure::Color: description->cColorBits = 16; break;
        case PixelFailure::Depth: description->cDepthBits = 16; break;
        case PixelFailure::None: break;
    }
    return result;
}

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
    if (!context) {
        ++detachCount;
    }
    return wglMakeCurrent(deviceContext, context);
}

BOOL deleteContext(HGLRC context)
{
    const BOOL deleted = wglDeleteContext(context);
    if (deleted && rejectDeletion) {
        // Освобождаем настоящий ресурс, затем имитируем ошибку для проверки списка диагностики.
        rejectDeletion = false;
        SetLastError(DeletionFailureCode);
        return FALSE;
    }
    return deleted;
}
}

int main()
{
    Platform::WindowOptions options;
    options.width = 320;
    options.height = 240;
    options.title = "Startup rollback test";
    std::vector<Platform::Error> errors;
    for (const auto failure : { PixelFailure::Read, PixelFailure::Window, PixelFailure::OpenGL,
        PixelFailure::DoubleBuffer, PixelFailure::Type, PixelFailure::Color, PixelFailure::Depth }) {
        pixelFailure = failure;
        failedWindow = nullptr;
        const bool started = Platform::initialize(options, errors);
        const char* operation = failure == PixelFailure::Read ? "DescribePixelFormat" : "ChoosePixelFormat";
        Platform::WindowState state;
        Platform::Error error;
        if (started || pixelFailure != PixelFailure::None || errors.size() != 1
            || errors.front().operation != operation || errors.front().message.empty()
            || !failedWindow || IsWindow(failedWindow)
            || Platform::getWindowState(state, error) || wglGetCurrentContext()) {
            std::cerr << "Invalid pixel format must fail and release the window.\n";
            PlatformTest::shutdownAfterFailure(errors);
            return 1;
        }
        if (failure == PixelFailure::Read && errors.front().nativeCode != static_cast<int>(ERROR_GEN_FAILURE)) {
            std::cerr << "DescribePixelFormat error code was lost.\n";
            return 1;
        }
        if (!Platform::initialize(options, errors)) {
            std::cerr << "Restart after pixel format rejection failed.\n";
            PlatformTest::shutdownAfterFailure(errors);
            return 1;
        }
        Platform::shutdown(errors);
        if (!errors.empty()) {
            PlatformTest::reportErrors(errors);
            return 1;
        }
    }
    for (const bool cleanupFailure : { false, true }) {
        rejectActivation = true;
        rejectDeletion = cleanupFailure;
        failedWindow = nullptr;
        detachCount = 0;
        const bool started = Platform::initialize(options, errors);
        if (started || errors.size() != (cleanupFailure ? 2U : 1U)
            || errors.front().operation != "wglMakeCurrent"
            || errors.front().nativeCode != static_cast<int>(ActivationFailureCode)
            || errors.front().message.empty()
            || (cleanupFailure && (errors[1].operation != "wglDeleteContext"
                || errors[1].nativeCode != static_cast<int>(DeletionFailureCode)
                || errors[1].message.empty()))) {
            std::cerr << "Startup must retain the original error followed by cleanup errors.\n";
            PlatformTest::shutdownAfterFailure(errors);
            return 1;
        }
        Platform::WindowState state;
        Platform::Error error;
        if (rejectActivation || rejectDeletion || detachCount != 0
            || !failedWindow || IsWindow(failedWindow)
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
        if (!errors.empty() || detachCount != 1 || !retryWindow || IsWindow(retryWindow)
            || wglGetCurrentContext() != nullptr || wglGetCurrentDC() != nullptr) {
            std::cerr << "Shutdown after a successful retry must release the window and detach the context.\n";
            PlatformTest::reportErrors(errors);
            return 1;
        }
    }
    return 0;
}
