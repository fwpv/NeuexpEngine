#include "app/platform.h"
#include "../vsync/vsync_driver.h"

#include <X11/Xlib.h>
#include <GL/glx.h>

#include <cstring>

namespace
{
constexpr int DriverFailureCode = 5;

int setInterval(int interval, const char* selected)
{
    auto& driver = VSyncTest::driver;
    ++driver.calls;
    driver.selected = selected;
    if (driver.fail) {
        return DriverFailureCode;
    }
    driver.interval = interval;
    return 0;
}

void swapEXT(Display* display, GLXDrawable, int interval)
{
    if (setInterval(interval, "GLX_EXT") != 0) {
        const XErrorHandler handler = XSetErrorHandler(nullptr);
        XSetErrorHandler(handler);
        XErrorEvent error = {};
        error.display = display;
        error.error_code = BadValue;
        handler(display, &error);
    }
}

int swapMESA(unsigned int interval)
{
    return setInterval(static_cast<int>(interval), "GLX_MESA");
}

int swapSGI(int interval)
{
    return setInterval(interval, "GLX_SGI");
}
}

namespace Platform::Testing
{
const char* queryVSyncExtensions(Display*, int)
{
    using namespace VSyncTest;
    if (driver.nullStrings) {
        return nullptr;
    }
    switch (driver.support) {
        case Support::Primary:
            return "GLX_EXT_swap_control";
        case Support::Secondary:
            return "GLX_MESA_swap_control";
        case Support::Legacy:
            return "GLX_SGI_swap_control";
        case Support::All:
            return "GLX_EXT_swap_control GLX_MESA_swap_control GLX_SGI_swap_control";
        case Support::SimilarName:
            return "GLX_EXT_swap_control_tear";
        default:
            return "";
    }
}

__GLXextFuncPtr lookupVSyncFunction(const GLubyte* name)
{
    const char* text = reinterpret_cast<const char*>(name);
    if (VSyncTest::driver.missingFunction || VSyncTest::driver.missingEntry == text) {
        return nullptr;
    }
    if (std::strcmp(text, "glXSwapIntervalEXT") == 0) {
        return reinterpret_cast<__GLXextFuncPtr>(swapEXT);
    }
    if (std::strcmp(text, "glXSwapIntervalMESA") == 0) {
        return reinterpret_cast<__GLXextFuncPtr>(swapMESA);
    }
    if (std::strcmp(text, "glXSwapIntervalSGI") == 0) {
        return reinterpret_cast<__GLXextFuncPtr>(swapSGI);
    }
    return nullptr;
}

Bool directVSyncContext(Display*, GLXContext)
{
    return VSyncTest::driver.direct ? True : False;
}
}

namespace VSyncTest
{
void runPlatformCases()
{
    Platform::Error error;
    reset();
    driver.support = Support::All;
    require(Platform::setVSync(true, error) && driver.selected == "GLX_EXT", "GLX_EXT did not take priority.");
    driver.missingEntry = "glXSwapIntervalEXT";
    require(Platform::setVSync(true, error) && driver.selected == "GLX_MESA",
        "Missing EXT entry point prevented MESA fallback.");
    driver.direct = false;
    require(Platform::setVSync(true, error) && driver.selected == "GLX_SGI",
        "Indirect MESA context prevented SGI fallback.");
    const int calls = driver.calls;
    require(!Platform::setVSync(false, error) && driver.calls == calls && driver.interval == 1,
        "Unavailable disable path changed the SGI interval.");
    reset();
    driver.fail = true;
    require(!Platform::setVSync(true, error) && error.operation == "glXSwapIntervalEXT"
        && error.nativeCode == BadValue, "Asynchronous GLX EXT failure was lost.");
    reset();
    driver.support = Support::Secondary;
    require(Platform::setVSync(true, error) && driver.selected == "GLX_MESA" && driver.interval == 1,
        "MESA enable fallback failed.");
    require(Platform::setVSync(false, error) && driver.interval == 0, "MESA disable fallback failed.");
    driver.fail = true;
    require(!Platform::setVSync(true, error) && error.operation == "glXSwapIntervalMESA"
        && error.nativeCode == DriverFailureCode && driver.interval == 0, "MESA failure was ignored.");
    reset();
    driver.support = Support::Secondary;
    driver.direct = false;
    require(!Platform::setVSync(true, error) && driver.calls == 0, "MESA was called for an indirect context.");

    reset();
    driver.support = Support::Legacy;
    require(Platform::setVSync(true, error) && driver.selected == "GLX_SGI" && driver.interval == 1,
        "SGI enable fallback failed.");
    require(!Platform::setVSync(false, error) && driver.calls == 1 && driver.interval == 1,
        "SGI accepted unsupported interval zero.");
    driver.fail = true;
    require(!Platform::setVSync(true, error) && error.operation == "glXSwapIntervalSGI"
        && error.nativeCode == DriverFailureCode && driver.interval == 1, "SGI failure was ignored.");
}
}
