#pragma once

#include <chrono>

namespace NeuexpEngine
{
// Монотонные часы кадра: переводят интервал между отсчётами в секунды.
class FrameClock
{
public:
    using Clock = std::chrono::steady_clock;

    // maxDeltaTime задаётся в секундах и должен быть конечным положительным числом.
    explicit FrameClock(float maxDeltaTime, Clock::time_point now = Clock::now());

    // Возвращает время с предыдущего отсчёта, ограниченное maxDeltaTime.
    // Переданный момент должен быть не раньше предыдущего отсчёта.
    // Предыдущий отсчёт заменяется на now даже при ограничении интервала.
    float tick(Clock::time_point now = Clock::now());

    // Начинает отсчёт заново, исключая время паузы из следующего интервала.
    void reset(Clock::time_point now = Clock::now());

private:
    float maxDeltaTime;
    Clock::time_point previousFrame;
};
}
