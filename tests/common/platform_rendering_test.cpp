#include "platform.h"
#include "platform_test_diagnostics.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <GL/glx.h>
#endif
#include <GL/gl.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
constexpr GLenum Framebuffer = 0x8D40;
constexpr GLenum ColorAttachment = 0x8CE0;
constexpr GLenum FramebufferComplete = 0x8CD5;
constexpr GLsizei ImageSize = 16;

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template<class Function>
Function loadFunction(const std::string& name)
{
#ifdef _WIN32
    const auto address = wglGetProcAddress(name.c_str());
    // Некоторые WGL-драйверы возвращают служебные значения вместо nullptr.
    const auto value = reinterpret_cast<std::intptr_t>(address);
    if (value == 0 || value == 1 || value == 2 || value == 3 || value == -1) {
        return nullptr;
    }
    return reinterpret_cast<Function>(reinterpret_cast<void (WINAPI*)()>(address));
#else
    const auto address = glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name.c_str()));
    return reinterpret_cast<Function>(address);
#endif
}

struct FramebufferApi
{
    using Generate = void (APIENTRY*)(GLsizei, GLuint*);
    using Bind = void (APIENTRY*)(GLenum, GLuint);
    using AttachTexture = void (APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
    using CheckStatus = GLenum (APIENTRY*)(GLenum);
    using Delete = void (APIENTRY*)(GLsizei, const GLuint*);

    Generate generate = nullptr;
    Bind bind = nullptr;
    AttachTexture attachTexture = nullptr;
    CheckStatus checkStatus = nullptr;
    Delete remove = nullptr;

    bool load()
    {
        const auto* versionText = glGetString(GL_VERSION);
        const auto* extensionText = glGetString(GL_EXTENSIONS);
        require(versionText != nullptr && extensionText != nullptr, "Could not query the OpenGL context.");
        int major = 0;
        std::istringstream version(reinterpret_cast<const char*>(versionText));
        version >> major;
        const std::string extensions = " " + std::string(reinterpret_cast<const char*>(extensionText)) + " ";
        const bool core = major >= 3 || extensions.find(" GL_ARB_framebuffer_object ") != std::string::npos;
        const bool ext = extensions.find(" GL_EXT_framebuffer_object ") != std::string::npos;
        if (!core && !ext) {
            return false;
        }
        const std::string suffix = core ? "" : "EXT";
        generate = loadFunction<Generate>("glGenFramebuffers" + suffix);
        bind = loadFunction<Bind>("glBindFramebuffer" + suffix);
        attachTexture = loadFunction<AttachTexture>("glFramebufferTexture2D" + suffix);
        checkStatus = loadFunction<CheckStatus>("glCheckFramebufferStatus" + suffix);
        remove = loadFunction<Delete>("glDeleteFramebuffers" + suffix);
        require(generate && bind && attachTexture && checkStatus && remove,
            "The driver advertises FBO support but does not expose its functions.");
        return true;
    }
};

struct RenderTarget
{
    const FramebufferApi& api;
    GLuint framebuffer = 0;
    GLuint texture = 0;

    explicit RenderTarget(const FramebufferApi& functions)
        : api(functions)
    {
    }

    ~RenderTarget()
    {
        api.bind(Framebuffer, 0);
        api.remove(1, &framebuffer);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &texture);
    }

    void initialize()
    {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ImageSize, ImageSize, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        api.generate(1, &framebuffer);
        api.bind(Framebuffer, framebuffer);
        api.attachTexture(Framebuffer, ColorAttachment, GL_TEXTURE_2D, texture, 0);
        glDrawBuffer(ColorAttachment);
        glReadBuffer(ColorAttachment);
        require(api.checkStatus(Framebuffer) == FramebufferComplete, "The test FBO is incomplete.");
        require(glGetError() == GL_NO_ERROR, "Could not initialize the test FBO.");
    }
};

int runTest()
{
    FramebufferApi api;
    if (!api.load()) {
        std::cout << "Pixel verification requires core/ARB/EXT framebuffer objects; "
            "window and presentation tests remain independent.\n";
        return 77;
    }

    // Скрытое окно исключает зависимость проверки FBO от видимости оконного буфера.
#ifdef _WIN32
    const HWND window = WindowFromDC(wglGetCurrentDC());
    require(window != nullptr, "Could not find the context window.");
    ShowWindow(window, SW_HIDE);
#else
    Display* display = glXGetCurrentDisplay();
    XUnmapWindow(display, glXGetCurrentDrawable());
    XSync(display, False);
#endif
    Platform::Error error;
    require(PlatformTest::checkOperation(Platform::processEvents(error), error), "Event processing failed.");

    RenderTarget target(api);
    target.initialize();
    glViewport(0, 0, ImageSize, ImageSize);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    for (const auto& color : { std::array<GLubyte, 3>{ 255, 0, 0 },
                              std::array<GLubyte, 3>{ 0, 255, 0 } }) {
        glClearColor(color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        std::array<GLubyte, 3> pixel = {};
        glReadPixels(ImageSize / 2, ImageSize / 2, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel.data());
        const GLenum renderError = glGetError();
        if (renderError != GL_NO_ERROR || pixel != color) {
            std::cerr << "FBO expected RGB=(" << static_cast<int>(color[0]) << ", "
                << static_cast<int>(color[1]) << ", " << static_cast<int>(color[2])
                << "), actual RGB=(" << static_cast<int>(pixel[0]) << ", "
                << static_cast<int>(pixel[1]) << ", " << static_cast<int>(pixel[2])
                << "); OpenGL error=" << renderError << '\n';
            throw std::runtime_error("The FBO does not contain the rendered color.");
        }
    }
    return 0;
}
}

int main()
{
#ifndef _WIN32
    const char* displayName = std::getenv("DISPLAY");
    if (!displayName || !*displayName) {
        std::cout << "Rendering tests require an X11 display.\n";
        return 77;
    }
#endif
    int result = 1;
    try {
        Platform::WindowOptions options;
        options.width = 64;
        options.height = 64;
        options.title = "Offscreen rendering test";
        Platform::Error failure;
        const bool initialized = Platform::initialize(options, failure);
        PlatformTest::reportError(failure);
        require(initialized, "Could not initialize the OpenGL context.");
        result = runTest();
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
    }
    Platform::Error failure;
    Platform::shutdown(failure);
    PlatformTest::reportError(failure);
    return failure.operation.empty() ? result : 1;
}
