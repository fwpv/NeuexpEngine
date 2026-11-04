#include "application.h"
#include "platform_test_diagnostics.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <GL/glx.h>
#endif
#include <GL/gl.h>

#ifdef None
#undef None
#endif

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
using namespace Engine;
constexpr Color Black = { 0, 0, 0 };

void require(bool success, const char* message)
{
    if (!success) {
        throw std::runtime_error(message);
    }
}

CameraView view()
{
    Camera2 camera;
    CameraView result;
    CameraError error;
    require(camera.createView({ 0, 0, 96, 96 }, result, error), "Camera initialization failed.");
    return result;
}

enum class Failure
{
    None,
    Start,
    StartException,
    Render,
    Drawing,
    RenderException,
    StopException,
    Initialize
};

class LifecycleApplication final : public Application
{
public:
    Failure failure = Failure::None;
    int stops = 0;
    bool stopHadContext = false;
    bool stopHadRenderer = false;
    Texture texture;

    bool rendererReady()
    {
        return getRenderer().isInitialized();
    }

    bool rejectedAfterStop()
    {
        RenderError error;
        getRenderer().beginFrame(96, 96, Black);
        return !getRenderer().endFrame(error) && error == RenderError::InvalidState;
    }

private:
    void queueFrame()
    {
        auto& drawing = getRenderer();
        beginFrame(96, 96, Black);
        drawing.setWorldView(view());
        DrawOptions draw;
        draw.color = { 1, 0, 0, 0.5F };
        drawing.drawRectangle({ -40, -40, 80, 80 }, draw);
        drawing.drawTexture(texture, { -20, -20, 40, 40 });
    }

    bool onStart(Platform::Error& error) override
    {
        RenderError renderError;
        if (failure == Failure::Initialize) {
#ifdef _WIN32
            const HDC surface = wglGetCurrentDC();
            const HGLRC context = wglGetCurrentContext();
            require(wglMakeCurrent(nullptr, nullptr) != FALSE, "Could not detach test context.");
#else
            Display* display = glXGetCurrentDisplay();
            const GLXDrawable drawable = glXGetCurrentDrawable();
            const GLXContext context = glXGetCurrentContext();
            require(glXMakeCurrent(display, 0, nullptr) != False, "Could not detach test context.");
#endif
            const bool rejected = !initializeRenderer(error) && !rendererReady();
#ifdef _WIN32
            require(wglMakeCurrent(surface, context) != FALSE, "Could not restore test context.");
#else
            require(glXMakeCurrent(display, drawable, context) != False, "Could not restore test context.");
#endif
            require(rejected, "Renderer initialized without a current context.");
            require(error.operation == "Renderer2D::initialize"
                && error.message == renderErrorMessage(RenderError::NoCurrentContext),
                "Initialization lost its rendering diagnostic.");
            return false;
        }
        require(getRenderer().initialize(renderError), "Renderer initialization failed.");
        require(!getRenderer().initialize(renderError) && renderError == RenderError::InvalidState,
            "Repeated renderer initialization was accepted.");
        ResourceError resourceError;
        require(getRenderer().createTexture({ 1, 1, { 255, 0, 0, 255 } }, texture, resourceError, true),
            "Could not create lifecycle texture.");
        if (failure == Failure::Start || failure == Failure::StartException) {
            queueFrame();
            if (failure == Failure::StartException) {
                throw std::runtime_error("Expected startup exception.");
            }
            error = { "onStart", "Expected startup failure.", {} };
            return false;
        }
        return true;
    }

    bool onRender(const FrameInfo&, Platform::Error& error) override
    {
        queueFrame();
        if (failure == Failure::RenderException) {
            throw std::runtime_error("Expected rendering exception.");
        }
        if (failure == Failure::Render) {
            error = { "onRender", "Expected rendering failure.", {} };
            return false;
        }
        if (failure == Failure::Drawing) {
            getRenderer().drawCircle({ {}, -1 });
            return endFrame(error);
        }
        require(endFrame(error), "Frame end failed.");
        requestExit();
        return true;
    }

    bool onStop(Platform::Error&) override
    {
        ++stops;
        stopHadContext = glGetString(GL_VERSION) != nullptr;
        stopHadRenderer = rendererReady();
        if (failure == Failure::StopException) {
            throw std::runtime_error("Expected cleanup exception.");
        }
        return true;
    }
};
}

int main()
{
#ifndef _WIN32
    if (!std::getenv("DISPLAY") || !*std::getenv("DISPLAY")) {
        return 77;
    }
#endif
    int result = 1;
    try {
        Renderer2D standalone;
        RenderError error;
        require(!standalone.initialize(error) && error == RenderError::NoCurrentContext,
            "Initialization without context succeeded.");
        standalone.shutdown();
        standalone.shutdown();
        for (Failure failure : { Failure::None, Failure::Start, Failure::StartException,
            Failure::Render, Failure::Drawing, Failure::RenderException, Failure::StopException, Failure::Initialize }) {
            LifecycleApplication app;
            app.failure = failure;
            ApplicationOptions options;
            options.window.width = 96;
            options.window.height = 96;
            options.targetFramesPerSecond = 0;
            Platform::Error operationError;
            const bool succeeded = app.run(options, operationError);
            if (succeeded != (failure == Failure::None)) {
                std::cerr << "Lifecycle scenario " << static_cast<int>(failure) << ":\n";
                PlatformTest::reportError(operationError);
            }
            require(succeeded == (failure == Failure::None), "Unexpected application result.");
            require(operationError.operation.empty() == succeeded, "Application lost or duplicated lifecycle errors.");
            if (failure == Failure::Drawing) {
                require(operationError.operation == "Renderer2D::endFrame"
                    && operationError.message == renderErrorMessage(RenderError::InvalidGeometry),
                    "Frame completion lost its rendering diagnostic.");
            }
            require(app.stops == 1 && app.stopHadContext, "Cleanup did not run before context destruction.");
            require(app.stopHadRenderer == (failure != Failure::Initialize), "Renderer lifetime ended before onStop.");
            require(!app.rendererReady() && app.rejectedAfterStop(), "Application left a live renderer after shutdown.");
            require(!app.texture.isValid() && app.texture.image() == nullptr,
                "Application retained a GPU texture or CPU image after shutdown.");
            require(!glGetString(GL_VERSION), "Application left a current context.");
        }
        // После очистки приложения допустим новый контекст и повторное использование рендерера.
        Platform::WindowOptions window;
        window.width = 96;
        window.height = 96;
        Platform::Error operationError;
        require(Platform::initialize(window, operationError), "Standalone context initialization failed.");
        for (int restart = 0; restart < 2; ++restart) {
            require(standalone.initialize(error), "Renderer restart failed.");
            standalone.beginFrame(96, 96, Black);
            standalone.setWorldView(view());
            standalone.drawCircle({ {}, 10 }, DrawOptions{});
            require(standalone.endFrame(error), "Restarted draw failed.");
            standalone.beginFrame(96, 96, Black);
            standalone.setWorldView(view());
            standalone.drawCircle({ {}, 10 }, DrawOptions{});
            standalone.shutdown();
            require(!standalone.isInitialized(), "Renderer shutdown failed.");
        }
        Platform::shutdown(operationError);
        require(operationError.operation.empty(), "Standalone context cleanup failed.");
        result = 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
    }
    Platform::Error operationError;
    Platform::shutdown(operationError);
    PlatformTest::reportError(operationError);
    return operationError.operation.empty() ? result : 1;
}
