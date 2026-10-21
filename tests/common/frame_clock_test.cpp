#include "frame_clock.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void requireNear(float actual, float expected)
{
    constexpr float Tolerance = 0.000001F;
    if (std::abs(actual - expected) > Tolerance) {
        throw std::runtime_error("Unexpected frame interval.");
    }
}
}

int main()
{
    try {
        using Clock = Engine::FrameClock::Clock;
        using std::chrono::milliseconds;
        const Clock::time_point start{};
        constexpr float MaximumDeltaTime = 0.08F;
        Engine::FrameClock clock(MaximumDeltaTime, start);
        requireNear(clock.tick(start), 0);
        requireNear(clock.tick(start + milliseconds(20)), 0.02F);
        requireNear(clock.tick(start + milliseconds(100)), MaximumDeltaTime);
        requireNear(clock.tick(start + milliseconds(1100)), MaximumDeltaTime);
        requireNear(clock.tick(start + milliseconds(1110)), 0.01F);
        clock.reset(start + milliseconds(5000));
        requireNear(clock.tick(start + milliseconds(5000)), 0);
        requireNear(clock.tick(start + milliseconds(5030)), 0.03F);
        clock.reset(start + milliseconds(10000));
        requireNear(clock.tick(start + milliseconds(10010)), 0.01F);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
