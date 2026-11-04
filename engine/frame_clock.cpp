#include "frame_clock.h"

#include <chrono>

namespace Engine
{
FrameClock::FrameClock(float maxDeltaTime, Clock::time_point now)
    : maxDeltaTime(maxDeltaTime)
    , previousFrame(now)
{
}

float FrameClock::tick(Clock::time_point now)
{
    const float deltaTime = std::chrono::duration<float>(now - previousFrame).count();
    previousFrame = now;
    return deltaTime > maxDeltaTime ? maxDeltaTime : deltaTime;
}

void FrameClock::reset(Clock::time_point now)
{
    previousFrame = now;
}
}
