#include "platform.h"

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
enum class Failure
{
    NoFailure,
    QueryLocale,
    ConfigureLocale,
    ConfigureModifiers,
    FallbackModifiers,
    RestoreLocale,
    RestoreModifiers
};

Failure failure = Failure::NoFailure;
int restoreFailures = 0;

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void checkRestored(const std::string& locale, const std::string& modifiers)
{
    const char* currentLocale = std::setlocale(LC_CTYPE, nullptr);
    require(currentLocale && locale == currentLocale, "Cleanup did not restore the original character locale.");
    const char* currentModifiers = XSetLocaleModifiers(nullptr);
    require(currentModifiers && modifiers == currentModifiers, "Cleanup did not restore the original locale modifiers.");
}

void checkStartup(Failure selected, const char* operation, const std::string& locale, const std::string& modifiers)
{
    failure = selected;
    Platform::Error error;
    Platform::WindowOptions options;
    require(!Platform::initialize(options, error), "Locale configuration failure was reported as success.");
    require(error.operation == operation && !error.message.empty(), "Locale configuration failure lost its diagnostic.");
    failure = Failure::NoFailure;
    require(Platform::shutdown(error), "Cleanup after rejected locale configuration failed.");
    checkRestored(locale, modifiers);
    require(Platform::initialize(options, error), "Initialization did not recover after locale configuration failure.");
    require(Platform::shutdown(error), "Cleanup after reinitialization failed.");
    checkRestored(locale, modifiers);
}

void checkCleanup(Failure selected, const char* operation, const std::string& locale, const std::string& modifiers)
{
    Platform::Error error;
    Platform::WindowOptions options;
    require(Platform::initialize(options, error), "Locale cleanup test initialization failed.");
    failure = selected;
    restoreFailures = 0;
    require(!Platform::shutdown(error) && error.operation == operation && !error.message.empty(),
        "Locale restoration failure was reported as success or lost its diagnostic.");
    require(restoreFailures == 1, "Cleanup did not attempt the selected locale operation.");
    require(!Platform::initialize(options, error) && error.operation == "initialize",
        "Initialization overwrote locale settings awaiting restoration.");
    require(!Platform::shutdown(error) && error.operation == operation && restoreFailures == 2,
        "Repeated cleanup forgot the locale restoration failure.");
    failure = Failure::NoFailure;
    require(Platform::shutdown(error), "Locale restoration did not recover after the failure was removed.");
    checkRestored(locale, modifiers);
    require(Platform::initialize(options, error), "Initialization did not recover after locale restoration.");
    require(Platform::shutdown(error), "Cleanup after locale recovery failed.");
    checkRestored(locale, modifiers);
}
}

extern "C" char* __real_setlocale(int, const char*);
extern "C" char* __wrap_setlocale(int category, const char* locale)
{
    if (category == LC_CTYPE) {
        if ((!locale && failure == Failure::QueryLocale)
            || (locale && *locale == '\0' && failure == Failure::ConfigureLocale)) {
            return nullptr;
        }
        if (locale && *locale != '\0' && failure == Failure::RestoreLocale) {
            ++restoreFailures;
            return nullptr;
        }
    }
    return __real_setlocale(category, locale);
}

extern "C" char* __real_XSetLocaleModifiers(const char*);
extern "C" char* __wrap_XSetLocaleModifiers(const char* modifiers)
{
    if (modifiers) {
        if ((*modifiers == '\0' && failure == Failure::ConfigureModifiers)
            || (std::strcmp(modifiers, "@im=none") == 0 && failure == Failure::FallbackModifiers)) {
            return nullptr;
        }
        if (*modifiers != '\0' && failure == Failure::RestoreModifiers) {
            ++restoreFailures;
            return nullptr;
        }
    }
    return __real_XSetLocaleModifiers(modifiers);
}

extern "C" XIM __real_XOpenIM(Display*, XrmDatabase, char*, char*);
extern "C" XIM __wrap_XOpenIM(Display* display, XrmDatabase database, char* name, char* className)
{
    return failure == Failure::FallbackModifiers ? nullptr : __real_XOpenIM(display, database, name, className);
}

int main()
{
    const char* displayName = std::getenv("DISPLAY");
    if (!displayName || *displayName == '\0') {
        return 77;
    }
    Platform::Error error;
    try {
        const char* currentLocale = std::setlocale(LC_CTYPE, nullptr);
        require(currentLocale != nullptr, "Could not read the test locale.");
        const std::string locale = currentLocale;
        const char* currentModifiers = XSetLocaleModifiers("@im=none");
        require(currentModifiers != nullptr, "Could not configure the test input method.");
        const std::string modifiers = currentModifiers;
        checkStartup(Failure::QueryLocale, "setlocale (query LC_CTYPE)", locale, modifiers);
        checkStartup(Failure::ConfigureLocale, "setlocale (LC_CTYPE)", locale, modifiers);
        checkStartup(Failure::ConfigureModifiers, "XSetLocaleModifiers", locale, modifiers);
        checkStartup(Failure::FallbackModifiers, "XSetLocaleModifiers (@im=none)", locale, modifiers);
        checkCleanup(Failure::RestoreLocale, "setlocale (restore LC_CTYPE)", locale, modifiers);
        checkCleanup(Failure::RestoreModifiers, "XSetLocaleModifiers (restore)", locale, modifiers);
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        Platform::reportError(error, false);
        failure = Failure::NoFailure;
        if (!Platform::shutdown(error)) {
            Platform::reportError(error, false);
        }
        return 1;
    }
}
