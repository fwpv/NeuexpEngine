#include "application_runtime.h"

#include <cmath>
#include <stdexcept>

namespace ApplicationTest
{
Runtime runtime;

void reset()
{
    runtime = {};
}

Clock::time_point Clock::now() noexcept
{
    return Clock::time_point(runtime.time);
}

void advance(std::chrono::milliseconds duration)
{
    runtime.time += duration;
}

void sleepUntil(Clock::time_point deadline)
{
    if (deadline > Clock::now()) {
        ++runtime.waits;
        runtime.time = deadline.time_since_epoch();
    }
}

void addEvent(Engine::EventType type)
{
    Engine::Event event;
    event.type = type;
    runtime.input.handleEvent(event);
}

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireNear(double actual, double expected)
{
    constexpr double Tolerance = 0.000001;
    require(std::isfinite(actual) && std::abs(actual - expected) <= Tolerance, "Unexpected time value.");
}
}

namespace Platform
{
namespace
{
bool operation(const char* name, Error& error)
{
    ApplicationTest::runtime.calls.emplace_back(name);
    if (ApplicationTest::runtime.failingOperation == name) {
        error = {name, "Injected failure.", 17};
        return false;
    }
    return true;
}
}

void reportError(const Error& error, bool)
{
    ApplicationTest::runtime.reportedErrors.push_back(error);
}

bool initialize(const WindowOptions&, Error& failure)
{
    auto& state = ApplicationTest::runtime;
    state.calls.emplace_back("initialize");
    failure = {};
    if (state.initializeFails) {
        failure = {"initialize", "Injected startup failure.", 11};
        return false;
    }
    state.initialized = true;
    state.closeRequested = state.closeDuringInitialize;
    if (state.closeRequested) {
        ApplicationTest::addEvent(Engine::EventType::CloseRequested);
    }
    return true;
}

bool processEvents(Error& error)
{
    auto& state = ApplicationTest::runtime;
    if (!operation("processEvents", error)) {
        return false;
    }
    ++state.polls;
    if (state.polls > 100) {
        error = { "processEvents", "Test frame limit exceeded.", {} };
        return false;
    }
    state.input.beginFrame();
    ApplicationTest::advance(std::chrono::milliseconds(1));
    if (state.events) {
        state.events();
    }
    return true;
}

bool isCloseRequested()
{
    return ApplicationTest::runtime.closeRequested;
}

bool getWindowState(WindowState& window, Error& error)
{
    if (!operation("getWindowState", error)) {
        return false;
    }
    window = ApplicationTest::runtime.window;
    return true;
}

bool present(Error& error)
{
    if (!operation("present", error)) {
        return false;
    }
    ++ApplicationTest::runtime.presents;
    ApplicationTest::advance(std::chrono::milliseconds(6));
    return true;
}

bool setVSync(bool enabled, Error& error)
{
    auto& state = ApplicationTest::runtime;
    state.calls.emplace_back(enabled ? "vsyncOn" : "vsyncOff");
    if (!state.initialized || !state.vsyncSupported || state.vsyncFails ||
        (!enabled && !state.vsyncDisableSupported)) {
        error = { "setVSync", "Injected VSync failure.", 23 };
        return false;
    }
    state.vsync = enabled;
    return true;
}

const Engine::InputState& getInput()
{
    return ApplicationTest::runtime.input;
}

const std::vector<Engine::Event>& getEvents()
{
    return ApplicationTest::runtime.input.events();
}

bool shutdown(Error& failure)
{
    auto& state = ApplicationTest::runtime;
    state.calls.emplace_back("shutdown");
    state.initialized = false;
    failure = {};
    if (state.shutdownFails) {
        failure = { "shutdown", "Injected cleanup failure.", 29 };
    }
    return failure.operation.empty();
}
}
