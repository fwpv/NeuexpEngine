#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include <GL/glx.h>
#endif
#include <GL/gl.h>

#ifdef None
#undef None
#endif

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>

// Отдельный буфер для пиксельных проверок, независимый от состояния окна.
// Расширения FBO используются только тестами, а не публичным API движка.
namespace RenderTest
{
constexpr GLenum Framebuffer = 0x8D40;
constexpr GLenum ColorAttachment = 0x8CE0;
constexpr GLenum FramebufferComplete = 0x8CD5;

inline void require(bool condition, const char* message)
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

    GLsizei size;

    RenderTarget(const FramebufferApi& functions, GLsizei size)
        : api(functions)
        , size(size)
    {
    }

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0,
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

}
