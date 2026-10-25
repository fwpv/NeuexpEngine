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
        return !getRenderer().beginFrame(96, 96, Black, error) && error == RenderError::InvalidState;
    }

private:
    void queueFrame()
    {
        RenderError error;
        auto& renderer = getRenderer();
        require(renderer.beginFrame(96, 96, Black, error) && renderer.beginWorld(view(), error),
            "Could not begin a lifecycle test frame.");
        DrawOptions draw;
        draw.color = { 1, 0, 0, 0.5F };
        require(renderer.drawRectangle({ -40, -40, 80, 80 }, ShapeFill::Filled, draw, error),
            "Could not queue lifecycle geometry.");
        require(renderer.drawTexture(texture, { -20, -20, 40, 40 }, {}, error),
            "Could not queue lifecycle texture.");
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
            const bool rejected = !getRenderer().initialize(renderError)
                && renderError == RenderError::NoCurrentContext && !rendererReady();
#ifdef _WIN32
            require(wglMakeCurrent(surface, context) != FALSE, "Could not restore test context.");
#else
            require(glXMakeCurrent(display, drawable, context) != False, "Could not restore test context.");
#endif
            require(rejected, "Renderer initialized without a current context.");
            error = { "renderer.initialize", "Expected initialization failure.", {} };
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
        RenderError renderError;
        require(getRenderer().endFrame(renderError), "Frame end failed.");
        requestExit();
        return true;
    }

    void onStop(std::vector<Platform::Error>&) override
    {
        ++stops;
        stopHadContext = glGetString(GL_VERSION) != nullptr;
        stopHadRenderer = rendererReady();
        if (failure == Failure::StopException) {
            throw std::runtime_error("Expected cleanup exception.");
        }
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
            Failure::Render, Failure::RenderException, Failure::StopException, Failure::Initialize }) {
            LifecycleApplication app;
            app.failure = failure;
            ApplicationOptions options;
            options.window.width = 96;
            options.window.height = 96;
            options.targetFramesPerSecond = 0;
            std::vector<Platform::Error> errors;
            const bool succeeded = app.run(options, errors);
            require(succeeded == (failure == Failure::None), "Unexpected application result.");
            require(errors.size() == (succeeded ? 0U : 1U), "Application lost or duplicated lifecycle errors.");
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
        std::vector<Platform::Error> errors;
        require(Platform::initialize(window, errors), "Standalone context initialization failed.");
        for (int restart = 0; restart < 2; ++restart) {
            require(standalone.initialize(error), "Renderer restart failed.");
            require(standalone.beginFrame(96, 96, Black, error), "Restarted renderer could not begin a frame.");
            require(standalone.beginWorld(view(), error), "Restarted world view failed.");
            require(standalone.drawCircle({ {}, 10 }, ShapeFill::Filled, {}, error), "Restarted draw failed.");
            standalone.shutdown();
            require(!standalone.isInitialized(), "Renderer shutdown failed.");
        }
        Platform::shutdown(errors);
        require(errors.empty(), "Standalone context cleanup failed.");
        result = 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
    }
    std::vector<Platform::Error> errors;
    Platform::shutdown(errors);
    PlatformTest::reportErrors(errors);
    return errors.empty() ? result : 1;
}
