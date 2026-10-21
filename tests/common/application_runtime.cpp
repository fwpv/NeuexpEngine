#include "application_runtime.h"

#include <cmath>
#include <stdexcept>
#include <thread>

namespace ApplicationTest
{
Runtime runtime;

void reset()
{
    runtime = {};
}

void advance(std::chrono::milliseconds duration)
{
#ifdef APPLICATION_VIRTUAL_TIME
    runtime.time += duration;
#else
    std::this_thread::sleep_for(duration);
#endif
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

bool virtualTime()
{
#ifdef APPLICATION_VIRTUAL_TIME
    return true;
#else
    return false;
#endif
}
}

#ifdef APPLICATION_VIRTUAL_TIME
extern "C" std::chrono::steady_clock::time_point __wrap__ZNSt6chrono3_V212steady_clock3nowEv() noexcept
{
    return std::chrono::steady_clock::time_point(ApplicationTest::runtime.time);
}

#ifdef _WIN32
extern "C" void __wrap__ZNSt11this_thread11__sleep_forENSt6chrono8durationIxSt5ratioILx1ELx1EEEENS1_IxS2_ILx1ELx1000000000EEEE(
    std::chrono::seconds seconds, std::chrono::nanoseconds nanoseconds)
{
    ++ApplicationTest::runtime.waits;
    ApplicationTest::runtime.time += seconds + nanoseconds;
}
#else
extern "C" int __wrap_nanosleep(const timespec* duration, timespec*)
{
    ++ApplicationTest::runtime.waits;
    ApplicationTest::runtime.time += std::chrono::seconds(duration->tv_sec)
        + std::chrono::nanoseconds(duration->tv_nsec);
    return 0;
}
#endif
#endif

namespace Platform
{
namespace
{
bool operation(const char* name, Error& error)
{
    ApplicationTest::runtime.calls.emplace_back(name);
    if (ApplicationTest::runtime.failingOperation == name) {
        error = { name, "Injected failure.", 17 };
        return false;
    }
    return true;
}
}

bool initialize(const WindowOptions&, std::vector<Error>& errors)
{
    auto& state = ApplicationTest::runtime;
    state.calls.emplace_back("initialize");
    errors.clear();
    if (state.initializeFails) {
        errors.push_back({ "initialize", "Injected startup failure.", 11 });
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

void shutdown(std::vector<Error>& errors)
{
    auto& state = ApplicationTest::runtime;
    state.calls.emplace_back("shutdown");
    state.initialized = false;
    errors.clear();
    if (state.shutdownFails) {
        errors.push_back({ "shutdown", "Injected cleanup failure.", 29 });
    }
}
}
