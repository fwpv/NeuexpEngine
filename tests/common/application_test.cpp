#include "application.h"
#include "application_runtime.h"
#include "events.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using ApplicationTest::advance;
using ApplicationTest::require;
using ApplicationTest::requireNear;
using ApplicationTest::runtime;
using Engine::FrameInfo;
using std::chrono::milliseconds;

struct CapturedFrame
{
    int number;
    FrameInfo info;
};

class ProbeApplication final : public Engine::Application
{
public:
    std::string exitStage;
    std::function<void(ProbeApplication&, const FrameInfo&)> inputAction;
    std::vector<CapturedFrame> inputs;
    std::vector<CapturedFrame> updates;
    std::vector<CapturedFrame> prepared;
    std::vector<CapturedFrame> rendered;

private:
    bool stage(const char* name, Platform::Error&)
    {
        runtime.calls.emplace_back(name);
        require(runtime.initialized, "Callback ran outside platform lifetime.");
        if (exitStage == name) {
            requestExit();
        }
        return true;
    }

    bool onStart(Platform::Error& error) override
    {
        return stage("start", error);
    }

    bool onInput(const FrameInfo& frame, Platform::Error& error) override
    {
        inputs.push_back({ runtime.polls, frame });
        if (!stage("input", error)) {
            return false;
        }
        if (inputAction) {
            inputAction(*this, frame);
        }
        advance(milliseconds(2));
        return true;
    }

    bool onUpdate(const FrameInfo& frame, Platform::Error& error) override
    {
        updates.push_back({ runtime.polls, frame });
        advance(milliseconds(3));
        return stage("update", error);
    }

    bool onPrepareFrame(const FrameInfo& frame, Platform::Error& error) override
    {
        prepared.push_back({ runtime.polls, frame });
        advance(milliseconds(4));
        return stage("prepare", error);
    }

    bool onRender(const FrameInfo& frame, Platform::Error& error) override
    {
        rendered.push_back({ runtime.polls, frame });
        advance(milliseconds(5));
        return stage("render", error);
    }

    bool onStop(Platform::Error&) override
    {
        runtime.calls.emplace_back("stop");
        require(runtime.initialized, "Application cleanup ran after platform shutdown.");
        return true;
    }
};

Engine::ApplicationOptions options()
{
    Engine::ApplicationOptions result;
    result.targetFramesPerSecond = 0;
    return result;
}

const FrameInfo& captured(const std::vector<CapturedFrame>& frames, int number)
{
    const auto found = std::find_if(frames.begin(), frames.end(), [number](const CapturedFrame& frame) {
        return frame.number == number;
    });
    require(found != frames.end(), "Expected callback was not invoked.");
    return found->info;
}

void requireCalls(const std::vector<std::string>& expected)
{
    require(runtime.calls == expected, "Unexpected application lifecycle order.");
}

void finishOnSecondInput(ProbeApplication& application)
{
    application.inputAction = [](auto& current, const auto&) {
        if (runtime.polls == 2) {
            current.requestExit();
        }
    };
}

void testLifecycle()
{
    ApplicationTest::reset();
    ProbeApplication application;
    finishOnSecondInput(application);
    Platform::Error failure{"old", "Old error.", {}};
    require(application.run(options(), failure), "Normal application failed.");
    require(failure.operation.empty(), "Successful run retained old errors.");
    requireCalls({ "initialize", "start", "processEvents", "getWindowState", "input",
        "update", "prepare", "render", "present", "processEvents", "getWindowState", "input", "stop", "shutdown" });
    const auto calls = runtime.calls;
    require(!application.run(options(), failure), "Repeated run was accepted.");
    require(runtime.calls == calls && !failure.operation.empty(), "Repeated run touched the platform.");

    for (const std::string exitStage : { "start", "input", "update", "prepare", "render" }) {
        ApplicationTest::reset();
        ProbeApplication exiting;
        exiting.exitStage = exitStage;
        require(exiting.run(options(), failure), "requestExit failed.");
        std::vector<std::string> expected{ "initialize", "start" };
        if (exitStage != "start") {
            expected.insert(expected.end(), { "processEvents", "getWindowState" });
            for (const std::string name : { "input", "update", "prepare", "render" }) {
                expected.push_back(name);
                if (name == exitStage) {
                    break;
                }
            }
        }
        expected.insert(expected.end(), { "stop", "shutdown" });
        requireCalls(expected);
        require(runtime.presents == 0, "An exiting callback was followed by presentation.");
    }
}

void testClosingEvents()
{
    for (int mode = 0; mode < 3; ++mode) {
        ApplicationTest::reset();
        runtime.closeDuringInitialize = mode == 1;
        runtime.events = [mode] {
            Engine::Event key;
            key.type = Engine::EventType::KeyPressed;
            key.key = Engine::Key::A;
            runtime.input.handleEvent(key);
            Engine::Event text;
            text.type = Engine::EventType::TextInput;
            text.text = "last";
            runtime.input.handleEvent(text);
            if (mode == 2) {
                runtime.window = {};
                ApplicationTest::addEvent(Engine::EventType::WindowDestroyed);
            }
            ApplicationTest::addEvent(Engine::EventType::CloseRequested);
            runtime.closeRequested = true;
        };
        ProbeApplication application;
        Engine::EventDispatcher dispatcher;
        std::vector<Engine::EventType> received;
        std::string receivedText;
        auto subscription = dispatcher.subscribe([&](const Engine::Event& event) {
            received.push_back(event.type);
            receivedText += event.text;
        });
        application.inputAction = [&](auto&, const auto&) {
            for (const auto& event : Platform::getEvents()) {
                dispatcher.dispatch(event);
            }
        };
        Platform::Error failure;
        require(application.run(options(), failure), "Closing application failed.");
        require(application.inputs.size() == 1, "Closing batch was not delivered exactly once.");
        require(application.updates.empty() && application.prepared.empty() && application.rendered.empty()
            && runtime.presents == 0, "Simulation or rendering continued after close.");
        require(!received.empty() && received.back() == Engine::EventType::CloseRequested, "Close event was lost.");
        if (mode == 1) {
            require(runtime.polls == 0, "Startup close batch was overwritten by another event poll.");
        } else {
            require(received[0] == Engine::EventType::KeyPressed && received[1] == Engine::EventType::TextInput
                && receivedText == "last", "Input preceding close was lost or reordered.");
        }
        if (mode == 2) {
            require(std::find(received.begin(), received.end(), Engine::EventType::WindowDestroyed) != received.end(),
                "Window destruction event was lost.");
        }
    }
}

void testPause()
{
    ApplicationTest::reset();
    runtime.events = [] {
        runtime.window.minimized = runtime.polls == 4 || runtime.polls == 7;
    };
    ProbeApplication application;
    application.inputAction = [](auto& current, const auto&) {
        if (runtime.polls == 2) {
            current.setPaused(true);
        }
        if (runtime.polls == 3) {
            advance(milliseconds(1000));
        }
        if (runtime.polls == 5) {
            require(current.isPaused(), "Restoring the window canceled explicit pause.");
        }
        if (runtime.polls == 6) {
            current.setPaused(false);
        }
        if (runtime.polls == 10) {
            current.requestExit();
        }
    };
    Platform::Error failure;
    require(application.run(options(), failure), "Pause scenario failed.");
    require(application.inputs.size() == 10, "Input stopped during pause or minimization.");
    require(application.updates.size() == 4, "Simulation ran while paused.");
    require(application.prepared.size() == 7 && application.rendered.size() == 7 && runtime.presents == 7,
        "Rendering did not follow pause and minimization states.");
    const double pausedTime = captured(application.rendered, 1).elapsedTime;
    for (int number : { 2, 3, 5 }) {
        const auto& frame = captured(application.rendered, number);
        require(frame.paused, "Explicit pause was not published.");
        requireNear(frame.deltaTime, 0);
        requireNear(frame.elapsedTime, pausedTime);
    }
    for (int number : { 6, 8 }) {
        const auto& frame = captured(application.updates, number);
        require(!frame.paused, "Resumed frame remained paused.");
        requireNear(frame.deltaTime, 0);
        requireNear(frame.elapsedTime, pausedTime);
    }
    const auto& running = captured(application.updates, 9);
    require(running.deltaTime > 0 && running.deltaTime <= options().maxDeltaTime, "Clock did not resume normally.");
    require(captured(application.inputs, 6).realTime > captured(application.inputs, 2).realTime,
        "Real time stopped during pause.");
}

void testVSync()
{
    Platform::Error failure;
    for (bool enabled : { false, true }) {
        ApplicationTest::reset();
        ProbeApplication application;
        finishOnSecondInput(application);
        auto config = options();
        config.verticalSync = enabled;
        require(application.run(config, failure) && runtime.vsync == enabled, "Initial VSync mode was not applied.");
        require(runtime.calls[1] == (enabled ? "vsyncOn" : "vsyncOff") && runtime.calls[2] == "start",
            "VSync was not configured before application startup.");
    }
    for (bool unsupported : { false, true }) {
        ApplicationTest::reset();
        runtime.vsyncSupported = !unsupported;
        runtime.vsyncFails = !unsupported;
        runtime.shutdownFails = true;
        ProbeApplication application;
        auto config = options();
        config.verticalSync = true;
        require(!application.run(config, failure), "Initial VSync failure was ignored.");
        requireCalls({ "initialize", "vsyncOn", "shutdown" });
        require(failure.operation == "setVSync",
            "VSync startup error was replaced by its cleanup failure.");
    }
    ApplicationTest::reset();
    runtime.vsyncSupported = false;
    ProbeApplication defaults;
    defaults.exitStage = "input";
    require(defaults.run(options(), failure), "Default settings required VSync support.");
    require(std::find(runtime.calls.begin(), runtime.calls.end(), "vsyncOn") == runtime.calls.end()
        && std::find(runtime.calls.begin(), runtime.calls.end(), "vsyncOff") == runtime.calls.end(),
        "Default settings changed the driver's mode.");
}

void testLimiter()
{
    ApplicationTest::reset();
    ProbeApplication application;
    application.inputAction = [](auto& current, const auto&) {
        Platform::Error error;
        if (runtime.polls == 2) {
            require(current.setVSync(true, error), "Runtime VSync enable failed.");
        }
        if (runtime.polls == 3) {
            runtime.vsyncFails = true;
            require(!current.setVSync(false, error) && runtime.vsync, "Failed mode change lost VSync state.");
            runtime.vsyncFails = false;
        }
        if (runtime.polls == 4) {
            runtime.vsyncDisableSupported = false;
            require(!current.setVSync(false, error) && runtime.vsync, "Unsupported disable lost VSync state.");
            runtime.vsyncDisableSupported = true;
        }
        if (runtime.polls == 5) {
            require(current.setVSync(false, error), "Runtime VSync disable failed.");
        }
        if (runtime.polls == 6) {
            current.requestExit();
        }
    };
    auto config = options();
    config.targetFramesPerSecond = 20;
    config.verticalSync = false;
    Platform::Error failure;
    require(application.run(config, failure), "Limiter scenario failed.");
    require(runtime.waits == 2, "VSync and software limiter ran together or limiter did not resume.");
    requireNear(captured(application.inputs, 2).timings.waiting, 0.029);
    for (int number : { 3, 4, 5 }) {
        requireNear(captured(application.inputs, number).timings.waiting, 0);
    }
    requireNear(captured(application.inputs, 6).timings.waiting, 0.029);

    for (double limit : { 0.0, 100.0 }) {
        ApplicationTest::reset();
        ProbeApplication unlimited;
        finishOnSecondInput(unlimited);
        auto settings = options();
        settings.targetFramesPerSecond = limit;
        require(unlimited.run(settings, failure) && runtime.waits == 0,
            "Disabled or expired frame limiter requested a wait.");
    }
    ApplicationTest::reset();
    ProbeApplication initiallySynchronized;
    finishOnSecondInput(initiallySynchronized);
    config.verticalSync = true;
    require(initiallySynchronized.run(config, failure) && runtime.waits == 0,
        "Initial VSync did not suppress the software limiter.");

    for (bool synchronized : { false, true }) {
        ApplicationTest::reset();
        runtime.events = [] { runtime.window.minimized = runtime.polls == 2; };
        ProbeApplication minimized;
        minimized.inputAction = [](auto& current, const auto&) {
            if (runtime.polls == 4) {
                current.requestExit();
            }
        };
        auto settings = options();
        settings.verticalSync = synchronized;
        require(minimized.run(settings, failure), "Minimized limiter failed.");
        require(runtime.waits == 1 && runtime.presents == 2, "Minimized polling depended on VSync or drew a frame.");
        require(captured(minimized.inputs, 3).timings.waiting > 0, "Minimized loop performed busy waiting.");
    }
}

void testStatistics()
{
    ApplicationTest::reset();
    runtime.events = [] { runtime.window.minimized = runtime.polls >= 13 && runtime.polls <= 15; };
    ProbeApplication application;
    bool measuredDuringPause = false;
    bool measuredAfterRestore = false;
    application.inputAction = [&](auto& current, const FrameInfo& frame) {
        if (runtime.polls == 5) {
            current.setPaused(true);
        }
        if (runtime.polls == 17) {
            current.setPaused(false);
        }
        if (frame.fpsUpdated && frame.framesPerSecond > 0) {
            require(frame.framesPerSecond > 5 && frame.framesPerSecond < 20, "FPS used simulation time or invalid frame count.");
            measuredDuringPause = measuredDuringPause || current.isPaused();
            measuredAfterRestore = measuredAfterRestore || runtime.polls > 17;
        }
        if (runtime.polls == 29) {
            current.requestExit();
        }
    };
    auto config = options();
    config.targetFramesPerSecond = 10;
    config.maxDeltaTime = 0.01F;
    Platform::Error failure;
    require(application.run(config, failure), "Statistics scenario failed.");
    require(measuredDuringPause && measuredAfterRestore, "FPS sampling stopped on pause or after restore.");
    const auto& reset = captured(application.prepared, 16);
    require(reset.fpsUpdated && reset.framesPerSecond == 0, "Restoration retained an old FPS sample.");
    const auto& first = captured(application.inputs, 1).timings;
    requireNear(first.total, 0);
    const auto& timing = captured(application.inputs, 2).timings;
    requireNear(timing.events, 0.003);
    requireNear(timing.update, 0.003);
    requireNear(timing.preparation, 0.004);
    requireNear(timing.rendering, 0.005);
    requireNear(timing.presentation, 0.006);
    requireNear(timing.waiting, 0.079);
    requireNear(timing.total, 0.1);
    for (const auto& frame : application.updates) {
        require(frame.info.deltaTime <= config.maxDeltaTime, "Frame delay exceeded the simulation limit.");
    }
}
}

int main(int argumentCount, char** arguments)
{
    try {
        require(argumentCount == 2, "Expected an application test group.");
        const std::string group = arguments[1];
        if (group == "lifecycle") {
            testLifecycle();
            testClosingEvents();
        } else if (group == "pause") {
            testPause();
        } else if (group == "vsync") {
            testVSync();
        } else if (group == "limiter") {
            testLimiter();
        } else if (group == "statistics") {
            testStatistics();
        } else {
            throw std::runtime_error("Unknown application test group.");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
