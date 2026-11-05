#include "platform.h"
#include "../common/platform_test_diagnostics.h"
#include "windows_gl_context.h"

#include <iostream>
#include <windows.h>
#include <initializer_list>

#undef wglMakeCurrent
#undef wglDeleteContext
#undef DescribePixelFormat

namespace
{
bool rejectActivation = false;
unsigned int detachCount = 0;
HWND failedWindow = nullptr;
constexpr DWORD ActivationFailureCode = ERROR_INVALID_HANDLE;
enum class PixelCase
{
    None, Read, Window, OpenGL, DoubleBuffer, Type, Color
};
PixelCase pixelCase = PixelCase::None;
}

namespace Platform::Testing
{
int describePixelFormat(HDC deviceContext, int format, UINT size, LPPIXELFORMATDESCRIPTOR description)
{
    const int result = DescribePixelFormat(deviceContext, format, size, description);
    if (pixelCase == PixelCase::None || result == 0) {
        return result;
    }
    failedWindow = WindowFromDC(deviceContext);
    const auto failure = pixelCase;
    pixelCase = PixelCase::None;
    switch (failure) {
        case PixelCase::Read: SetLastError(ERROR_GEN_FAILURE); return 0;
        case PixelCase::Window: description->dwFlags &= ~static_cast<DWORD>(PFD_DRAW_TO_WINDOW); break;
        case PixelCase::OpenGL: description->dwFlags &= ~static_cast<DWORD>(PFD_SUPPORT_OPENGL); break;
        case PixelCase::DoubleBuffer: description->dwFlags &= ~static_cast<DWORD>(PFD_DOUBLEBUFFER); break;
        case PixelCase::Type: description->iPixelType = PFD_TYPE_COLORINDEX; break;
        case PixelCase::Color: description->cColorBits = 16; break;
        case PixelCase::None: break;
    }
    return result;
}

// Эта реализация используется только в тестовой копии платформы.
// При обычном запуске и последующем успешном повторе вызывается настоящий wglMakeCurrent.
BOOL makeContextCurrent(HDC deviceContext, HGLRC context)
{
    if (context && rejectActivation) {
        rejectActivation = false;
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
    return wglDeleteContext(context);
}
}

int main()
{
    Platform::WindowOptions options;
    options.width = 320;
    options.height = 240;
    options.title = "Startup rollback test";
    Platform::Error failure;
    for (const auto pixelFailure : {PixelCase::Read, PixelCase::Window, PixelCase::OpenGL,
        PixelCase::DoubleBuffer, PixelCase::Type}) {
        pixelCase = pixelFailure;
        failedWindow = nullptr;
        const bool started = Platform::initialize(options, failure);
        const char* operation = pixelFailure == PixelCase::Read ? "DescribePixelFormat" : "ChoosePixelFormat";
        Platform::WindowState state;
        Platform::Error error;
        if (started || pixelCase != PixelCase::None || failure.operation.empty()
            || failure.operation != operation || failure.message.empty()
            || !failedWindow || IsWindow(failedWindow)
            || Platform::getWindowState(state, error) || wglGetCurrentContext()) {
            std::cerr << "Invalid pixel format must fail and release the window.\n";
            PlatformTest::reportError(failure);
            Platform::shutdown(failure);
            return 1;
        }
        if (pixelFailure == PixelCase::Read && failure.nativeCode != static_cast<int>(ERROR_GEN_FAILURE)) {
            std::cerr << "DescribePixelFormat error code was lost.\n";
            return 1;
        }
        if (!Platform::initialize(options, failure)) {
            std::cerr << "Restart after pixel format rejection failed.\n";
            PlatformTest::reportError(failure);
            Platform::shutdown(failure);
            return 1;
        }
        Platform::shutdown(failure);
        if (!failure.operation.empty()) {
            PlatformTest::reportError(failure);
            return 1;
        }
    }
    // Подменяется описание формата; формат драйвера не меняется.
    for (const auto alternative : {PixelCase::Color}) {
        pixelCase = alternative;
        const bool started = Platform::initialize(options, failure);
        Platform::Error error;
        if (!started || !failure.operation.empty() || pixelCase != PixelCase::None
            || !Platform::present(error)) {
            std::cerr << "Alternative color precision must allow rendering.\n";
            PlatformTest::reportError(failure);
            Platform::shutdown(failure);
            return 1;
        }
        Platform::shutdown(failure);
        if (!failure.operation.empty()) {
            PlatformTest::reportError(failure);
            return 1;
        }
    }
    {
        rejectActivation = true;
        failedWindow = nullptr;
        detachCount = 0;
        const bool started = Platform::initialize(options, failure);
        if (started || failure.operation != "wglMakeCurrent"
            || failure.nativeCode != static_cast<int>(ActivationFailureCode) || failure.message.empty()) {
            std::cerr << "Startup must retain the original error followed by cleanup errors.\n";
            PlatformTest::reportError(failure);
            Platform::shutdown(failure);
            return 1;
        }
        Platform::WindowState state;
        Platform::Error error;
        if (rejectActivation || detachCount != 0
            || !failedWindow || IsWindow(failedWindow)
            || Platform::getWindowState(state, error) || Platform::isCloseRequested()
            || wglGetCurrentContext() != nullptr || wglGetCurrentDC() != nullptr) {
            std::cerr << "Failed initialization must destroy its window and leave no usable platform state.\n";
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
            || wglGetCurrentContext() == nullptr || wglGetCurrentDC() == nullptr) {
            std::cerr << "Initialization after automatic rollback must succeed and clear old errors.\n";
            PlatformTest::reportError(failure);
            Platform::shutdown(failure);
            return 1;
        }
        const HWND retryWindow = WindowFromDC(wglGetCurrentDC());
        Platform::shutdown(failure);
        if (!failure.operation.empty() || detachCount != 1 || !retryWindow || IsWindow(retryWindow)
            || wglGetCurrentContext() != nullptr || wglGetCurrentDC() != nullptr) {
            std::cerr << "Shutdown after a successful retry must release the window and detach the context.\n";
            PlatformTest::reportError(failure);
            return 1;
        }
    }
    return 0;
}
