#include "platform.h"
#include "vsync_driver.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace VSyncTest
{
Driver driver;

void reset()
{
    driver = {};
}

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}
}

namespace
{
void runCommonCases()
{
    using namespace VSyncTest;
    Platform::Error error;
    reset();
    require(Platform::setVSync(true, error) && driver.interval == 1 && driver.calls == 1,
        "Enabling VSync did not request interval 1.");
    require(Platform::setVSync(false, error) && driver.interval == 0 && driver.calls == 2,
        "Disabling VSync did not request interval 0.");
    driver.fail = true;
    require(!Platform::setVSync(true, error) && driver.interval == 0 && driver.calls == 3,
        "Driver failure was ignored or changed the active interval.");
    require(!error.operation.empty() && !error.message.empty() && error.nativeCode.has_value(),
        "Driver failure lost its native diagnostic.");
    driver.fail = false;
    require(Platform::setVSync(true, error) && driver.interval == 1,
        "A previous driver failure prevented recovery.");

    for (Support support : { Support::Unavailable, Support::SimilarName }) {
        reset();
        driver.support = support;
        require(!Platform::setVSync(true, error) && driver.calls == 0 && !error.message.empty(),
            "Unavailable extension was used.");
        require(!Platform::setVSync(false, error) && driver.calls == 0,
            "Disabling unsupported VSync was reported as successful.");
    }
    reset();
    driver.missingFunction = true;
    require(!Platform::setVSync(true, error) && driver.calls == 0,
        "Missing extension entry point was used.");
    reset();
    driver.nullStrings = true;
    require(!Platform::setVSync(true, error) && driver.calls == 0,
        "Null extension string was treated as support.");
}
}

int main()
{
    std::vector<Platform::Error> errors;
    bool passed = false;
    try {
        Platform::Error error;
        VSyncTest::require(!Platform::setVSync(true, error) && VSyncTest::driver.calls == 0,
            "VSync was available before platform initialization.");
#ifndef _WIN32
        const char* display = std::getenv("DISPLAY");
        if (!display || !*display) {
            std::cout << "Platform VSync tests require an X11 display.\n";
            return 77;
        }
#endif
        Platform::WindowOptions options;
        options.title = "Engine VSync test";
        options.width = 320;
        options.height = 240;
        if (!Platform::initialize(options, errors)) {
            for (const auto& startupError : errors) {
                Platform::reportError(startupError, false);
            }
            throw std::runtime_error("VSync test platform initialization failed.");
        }
        runCommonCases();
        VSyncTest::runPlatformCases();
        passed = true;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    Platform::shutdown(errors);
    for (const auto& error : errors) {
        Platform::reportError(error, false);
    }
    Platform::Error error;
    const int calls = VSyncTest::driver.calls;
    if (Platform::setVSync(false, error) || VSyncTest::driver.calls != calls) {
        std::cerr << "VSync remained available after shutdown.\n";
        passed = false;
    }
    return passed && errors.empty() ? 0 : 1;
}
