#include "platform.h"
#include "../vsync/vsync_driver.h"

#include <windows.h>

#include <cstring>

namespace
{
const char* extensionNames()
{
    using namespace VSyncTest;
    if (driver.nullStrings) {
        return nullptr;
    }
    switch (driver.support) {
        case Support::Unavailable:
            return "";
        case Support::SimilarName:
            return "WGL_EXT_swap_control_tear";
        default:
            return "WGL_ARB_extensions_string WGL_EXT_swap_control WGL_EXT_extensions_string";
    }
}

const char* WINAPI extensionsARB(HDC)
{
    return extensionNames();
}

const char* WINAPI extensionsEXT()
{
    return extensionNames();
}

BOOL WINAPI swapInterval(int interval)
{
    auto& driver = VSyncTest::driver;
    ++driver.calls;
    driver.selected = "WGL_EXT";
    if (driver.fail) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    driver.interval = interval;
    return TRUE;
}

template<class Function>
PROC procedure(Function function)
{
    return reinterpret_cast<PROC>(reinterpret_cast<void (WINAPI*)()>(function));
}
}

namespace Platform::Testing
{
PROC WINAPI lookupVSyncFunction(LPCSTR name)
{
    const auto& driver = VSyncTest::driver;
    if (std::strcmp(name, "wglGetExtensionsStringARB") == 0) {
        return driver.arbString ? procedure(extensionsARB) : nullptr;
    }
    if (std::strcmp(name, "wglGetExtensionsStringEXT") == 0) {
        return driver.extString ? procedure(extensionsEXT) : nullptr;
    }
    if (std::strcmp(name, "wglSwapIntervalEXT") == 0) {
        if (driver.invalidAddress != 0) {
            return reinterpret_cast<PROC>(driver.invalidAddress);
        }
        return driver.missingFunction ? nullptr : procedure(swapInterval);
    }
    return nullptr;
}
}

namespace VSyncTest
{
void runPlatformCases()
{
    Platform::Error error;
    reset();
    driver.arbString = false;
    require(Platform::setVSync(true, error) && driver.interval == 1,
        "WGL_EXT_extensions_string fallback failed.");
    driver.extString = false;
    require(!Platform::setVSync(false, error) && driver.interval == 1 && driver.calls == 1,
        "Absent WGL extension queries changed VSync.");
    for (std::intptr_t invalid : { 1, 2, 3, -1 }) {
        reset();
        driver.invalidAddress = invalid;
        require(!Platform::setVSync(true, error) && driver.calls == 0,
            "Invalid WGL procedure address was used.");
    }
    reset();
    driver.fail = true;
    require(!Platform::setVSync(true, error) && error.operation == "wglSwapIntervalEXT"
        && error.nativeCode == static_cast<int>(ERROR_INVALID_DATA), "WGL native error was lost.");
}
}
