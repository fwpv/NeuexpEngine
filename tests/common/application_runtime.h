#pragma once

#include "platform.h"

#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace ApplicationTest
{
struct Clock
{
    using duration = std::chrono::steady_clock::duration;
    using time_point = std::chrono::steady_clock::time_point;

    static time_point now() noexcept;
};

struct Runtime
{
    Clock::duration time = std::chrono::seconds(1);
    std::vector<std::string> calls;
    Engine::InputState input;
    Platform::WindowState window{ 320, 240, true, false };
    std::function<void()> events;
    bool initialized = false;
    bool closeRequested = false;
    bool vsync = false;
    bool vsyncSupported = true;
    bool vsyncFails = false;
    bool vsyncDisableSupported = true;
    bool closeDuringInitialize = false;
    bool initializeFails = false;
    bool shutdownFails = false;
    std::string failingOperation;
    int polls = 0;
    int presents = 0;
    int waits = 0;
};

extern Runtime runtime;
void reset();
void advance(std::chrono::milliseconds duration);
void sleepUntil(Clock::time_point deadline);
void addEvent(Engine::EventType type);
void require(bool condition, const char* message);
void requireNear(double actual, double expected);
}
