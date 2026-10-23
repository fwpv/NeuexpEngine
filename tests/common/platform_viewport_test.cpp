#include "platform.h"
#include "platform_test_diagnostics.h"

#ifdef _WIN32
#include <windows.h>
#include "../windows/window_frame.h"
#else
#include <GL/glx.h>
#endif
#include <GL/gl.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

namespace
{
using Viewport = std::array<GLint, 4>;
using Matrix = std::array<GLdouble, 16>;

bool checkViewport(const Viewport& expected)
{
    Viewport actual = {};
    glGetIntegerv(GL_VIEWPORT, actual.data());
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR || actual != expected) {
        std::cerr << "Expected viewport (" << expected[0] << ", " << expected[1] << ", "
            << expected[2] << ", " << expected[3] << "), got ("
            << actual[0] << ", " << actual[1] << ", " << actual[2] << ", " << actual[3]
            << "); OpenGL error=" << error << '\n';
        return false;
    }
    return true;
}

bool resizeWindow(int width, int height, Platform::Error& error)
{
#ifdef _WIN32
    const HWND window = WindowFromDC(wglGetCurrentDC());
    const auto style = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
    const auto extendedStyle = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE));
    RECT bounds = { 0, 0, width, height };
    if (!PlatformTest::adjustWindowFrame(window, bounds, style, extendedStyle)) {
        error = { "AdjustWindowRectExForDpi", "Could not calculate the resized frame.", static_cast<int>(GetLastError()) };
        return false;
    }
    if (!SetWindowPos(window, nullptr, 0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE)) {
        error = { "SetWindowPos", "Could not resize the test window.", static_cast<int>(GetLastError()) };
        return false;
    }
#else
    Display* display = glXGetCurrentDisplay();
    XResizeWindow(display, glXGetCurrentDrawable(), static_cast<unsigned int>(width),
        static_cast<unsigned int>(height));
    XSync(display, False);
    (void)error;
#endif
    return true;
}

bool runTest()
{
    constexpr int InitialWidth = 320;
    constexpr int InitialHeight = 240;
    Platform::WindowOptions options;
    options.width = InitialWidth;
    options.height = InitialHeight;
    options.title = "Viewport test";
    std::vector<Platform::Error> errors;
    if (!Platform::initialize(options, errors)) {
        PlatformTest::reportErrors(errors);
        return false;
    }
    if (!checkViewport({ 0, 0, InitialWidth, InitialHeight })) {
        return false;
    }

    // Фиксированная проекция игрового мира и преобразование объекта не зависят от размеров окна.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    constexpr double WorldWidth = 800.0;
    constexpr double WorldHeight = 600.0;
    glOrtho(0.0, WorldWidth, WorldHeight, 0.0, -1.0, 1.0);
    Matrix projection = {};
    glGetDoublev(GL_PROJECTION_MATRIX, projection.data());
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(10.0f, 20.0f, 0.0f);
    Matrix modelView = {};
    glGetDoublev(GL_MODELVIEW_MATRIX, modelView.data());

    Platform::Error error;
    const std::array<std::array<int, 2>, 4> sizes = {{
        { 400, 300 }, { 400, 360 }, { 480, 360 }, { InitialWidth, InitialHeight }
    }};
    for (const auto& size : sizes) {
        // При отсутствии изменения размера обработка событий не должна сбрасывать viewport приложения.
        const Viewport customViewport = { 3, 5, 120, 90 };
        glViewport(customViewport[0], customViewport[1], customViewport[2], customViewport[3]);
        if (!PlatformTest::checkOperation(Platform::processEvents(error), error)
            || !checkViewport(customViewport)
            || !PlatformTest::checkOperation(resizeWindow(size[0], size[1], error), error)) {
            return false;
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        Platform::WindowState state;
        do {
            if (!PlatformTest::checkOperation(Platform::processEvents(error), error)
                || !PlatformTest::checkOperation(Platform::getWindowState(state, error), error)) {
                return false;
            }
            if (state.width == size[0] && state.height == size[1]) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while (std::chrono::steady_clock::now() < deadline);
        if (state.width != size[0] || state.height != size[1]) {
            std::cerr << "The window did not reach " << size[0] << 'x' << size[1] << ".\n";
            return false;
        }
        if (!checkViewport({ 0, 0, size[0], size[1] })) {
            return false;
        }
        Matrix actualProjection = {};
        Matrix actualModelView = {};
        GLint matrixMode = 0;
        glGetDoublev(GL_PROJECTION_MATRIX, actualProjection.data());
        glGetDoublev(GL_MODELVIEW_MATRIX, actualModelView.data());
        glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
        if (glGetError() != GL_NO_ERROR || actualProjection != projection
            || actualModelView != modelView || matrixMode != GL_MODELVIEW) {
            std::cerr << "Resizing changed the world matrices or the selected matrix mode.\n";
            return false;
        }
    }
    return true;
}
}

int main()
{
#ifndef _WIN32
    const char* displayName = std::getenv("DISPLAY");
    if (!displayName || !*displayName) {
        std::cout << "Viewport tests require an X11 display.\n";
        return 77;
    }
#endif
    const bool passed = runTest();
    std::vector<Platform::Error> errors;
    Platform::shutdown(errors);
    PlatformTest::reportErrors(errors);
    return passed && errors.empty() ? 0 : 1;
}
