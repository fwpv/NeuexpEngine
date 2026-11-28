#include "application.h"
#include "frame_clock.h"
#include "detail/cleanup_error.h"
#include "core/math2d.h"
#include "platform.h"

#include <chrono>
#include <cmath>
#include <exception>
#include <thread>
#include <utility>
#include <optional>

namespace NeuexpEngine
{
namespace
{
using Clock = FrameClock::Clock;
constexpr double FpsIntervalSeconds = 1.0;
constexpr double MinimizedFramesPerSecond = 30.0;

double secondsBetween(Clock::time_point start, Clock::time_point end)
{
    return std::chrono::duration<double>(end - start).count();
}
}

bool Application::run(const ApplicationOptions& options, Platform::Error& error)
{
    error = {};
    if (started) {
        error = Platform::Error{"Application::run", "Application has already been started.", std::nullopt};
        return false;
    }
    if (!std::isfinite(options.maxDeltaTime) || options.maxDeltaTime <= 0 ||
        !std::isfinite(options.targetFramesPerSecond) || options.targetFramesPerSecond < 0 ||
        !std::isfinite(options.fixedStepTime) || options.fixedStepTime < 0 ||
        options.maxCatchUpSteps < 1) {
        error = Platform::Error{"Application::run", "Invalid frame timing options.", std::nullopt};
        return false;
    }
    started = true;
    bool success = false;
    bool startAttempted = false;
    try {
        if (!Platform::initialize(options.window, error)) {
            return false;
        }
        success = !options.verticalSync || setVSync(*options.verticalSync, error);
        if (success) {
            startAttempted = true;
            success = onStart(error);
        }
        if (success) {
            success = runFrames(options, error);
        }
    } catch (const std::exception& exception) {
        error = Platform::Error{"Application::run", exception.what(), std::nullopt};
        success = false;
    } catch (...) {
        error = Platform::Error{"Application::run", "Unknown exception in application callback.", std::nullopt};
        success = false;
    }

    if (success) {
        error = {};
    }
    Platform::Error cleanupError;
    try {
        if (startAttempted && !onStop(cleanupError)) {
            Platform::Detail::keepFirstError(error, std::move(cleanupError));
            success = false;
        }
    } catch (const std::exception& exception) {
        Platform::Detail::keepFirstError(error, Platform::Error{"Application::onStop", exception.what(), std::nullopt});
        success = false;
    } catch (...) {
        Platform::Detail::keepFirstError(error, Platform::Error{"Application::onStop", "Unknown exception during cleanup.", std::nullopt});
        success = false;
    }
    graphics.shutdown();
    if (!Platform::shutdown(cleanupError)) {
        Platform::Detail::keepFirstError(error, std::move(cleanupError));
        success = false;
    }
    return success;
}

bool Application::runFrames(const ApplicationOptions& options, Platform::Error& error)
{
    const auto start = Clock::now();
    const bool fixedStep = options.fixedStepTime > 0;
    FrameClock frameClock(options.maxDeltaTime, start);
    // Время кадра, не поместившееся в целое число шагов постоянной длительности.
    double fixedAccumulator = 0;
    auto fpsStart = start;
    double presentedFrames = 0;
    FrameInfo frame;
    bool wasPaused = paused;
    bool wasMinimized = false;
    while (!exitRequested) {
        const auto frameStart = Clock::now();
        FrameTimings timings;
        if (!Platform::isCloseRequested() && !Platform::processEvents(error)) {
            return false;
        }
        if (!Platform::getWindowState(frame.window, error)) {
            return false;
        }
        frame.realTime = secondsBetween(start, frameStart);
        frame.paused = paused || frame.window.minimized;
        if (!onInput(frame, error)) {
            return false;
        }
        if (exitRequested || Platform::isCloseRequested()) {
            break;
        }
        auto stageEnd = Clock::now();
        timings.events = secondsBetween(frameStart, stageEnd);

        frame.paused = paused || frame.window.minimized;
        // Первый кадр после продолжения имеет dT=0; после паузы и сворачивания накопитель
        // постоянного шага тоже сбрасывается, поэтому симуляция не прыгает вперёд.
        if (frame.paused || wasPaused) {
            frameClock.reset(frameStart);
            frame.deltaTime = 0;
            fixedAccumulator = 0;
        } else {
            frame.deltaTime = frameClock.tick(frameStart);
        }
        wasPaused = frame.paused;
        // Переменный dT даёт одно обновление за кадр, постоянный шаг — столько, сколько
        // шагов накопилось, но не больше предела догоняющих шагов.
        int updateCount = 0;
        if (!frame.paused) {
            updateCount = 1;
            if (fixedStep) {
                fixedAccumulator += frame.deltaTime;
                updateCount = static_cast<int>(fixedAccumulator / options.fixedStepTime);
                if (updateCount > options.maxCatchUpSteps) {
                    updateCount = options.maxCatchUpSteps;
                    fixedAccumulator = 0;
                } else {
                    fixedAccumulator -= static_cast<double>(updateCount) * options.fixedStepTime;
                }
                frame.deltaTime = options.fixedStepTime;
            }
        }
        frame.updates = updateCount;

        if (frame.window.minimized || wasMinimized) {
            fpsStart = frameStart;
            presentedFrames = 0;
            frame.framesPerSecond = 0;
            frame.fpsUpdated = true;
        }
        wasMinimized = frame.window.minimized;

        const auto updateStart = stageEnd;
        for (int index = 0; index < updateCount; ++index) {
            // Каждый шаг видит игровое время, включающее только выполненные шаги.
            frame.elapsedTime += static_cast<double>(frame.deltaTime);
            if (!onUpdate(frame, error)) {
                return false;
            }
            if (exitRequested) {
                break;
            }
        }
        stageEnd = Clock::now();
        timings.update = secondsBetween(updateStart, stageEnd);
        if (exitRequested) {
            break;
        }

        if (!frame.window.minimized) {
            const auto prepareStart = stageEnd;
            if (!onPrepareFrame(frame, error)) {
                return false;
            }
            stageEnd = Clock::now();
            timings.preparation = secondsBetween(prepareStart, stageEnd);
            if (exitRequested) {
                break;
            }
            const auto renderStart = stageEnd;
            if (!onRender(frame, error)) {
                return false;
            }
            stageEnd = Clock::now();
            timings.rendering = secondsBetween(renderStart, stageEnd);
            if (exitRequested) {
                break;
            }
            const auto presentStart = stageEnd;
            if (!Platform::present(error)) {
                return false;
            }
            stageEnd = Clock::now();
            timings.presentation = secondsBetween(presentStart, stageEnd);
            ++presentedFrames;
        }

        // FPS использует число показанных кадров и реальное время.
        frame.fpsUpdated = false;
        const double fpsElapsed = secondsBetween(fpsStart, stageEnd);
        if (!frame.window.minimized && fpsElapsed >= FpsIntervalSeconds) {
            frame.framesPerSecond = presentedFrames / fpsElapsed;
            frame.fpsUpdated = true;
            fpsStart = stageEnd;
            presentedFrames = 0;
        }

        // Ожидание учитывает все стадии кадра, включая present().
        const double targetFps = frame.window.minimized ?
            MinimizedFramesPerSecond : (verticalSyncEnabled ? 0.0 : options.targetFramesPerSecond);
        const auto waitStart = Clock::now();
        if (targetFps > 0) {
            const auto targetDuration = std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(1.0 / targetFps));
            std::this_thread::sleep_until(frameStart + targetDuration);
        }
        const auto frameEnd = Clock::now();
        timings.waiting = secondsBetween(waitStart, frameEnd);
        timings.total = secondsBetween(frameStart, frameEnd);
        frame.timings = timings;
    }
    return true;
}

void Application::setPaused(bool value)
{
    paused = value;
}

bool Application::isPaused() const
{
    return paused;
}

bool Application::setVSync(bool enabled, Platform::Error& error)
{
    if (!Platform::setVSync(enabled, error)) {
        return false;
    }
    verticalSyncEnabled = enabled;
    return true;
}

void Application::requestExit()
{
    exitRequested = true;
}

bool Application::onStart(Platform::Error&)
{
    return true;
}

Renderer2D& Application::getRenderer()
{
    return graphics;
}

bool Application::initializeRenderer(Platform::Error& error)
{
    RenderError renderError;
    const bool success = graphics.initialize(renderError);
    return rendererResult("Renderer2D::initialize", success, renderError, error);
}

void Application::beginFrame(int width, int height, Color clear)
{
    graphics.beginFrame(width, height, clear);
}

bool Application::endFrame(Platform::Error& error)
{
    RenderError renderError;
    const bool success = graphics.endFrame(renderError);
    return rendererResult("Renderer2D::endFrame", success, renderError, error);
}

bool Application::rendererResult(const char* operation, bool success, RenderError error, Platform::Error& result) const
{
    result = {};
    if (!success) {
        result = {operation, renderErrorMessage(error), std::nullopt};
    }
    return success;
}

bool Application::onInput(const FrameInfo&, Platform::Error&)
{
    return true;
}

bool Application::onUpdate(const FrameInfo&, Platform::Error&)
{
    return true;
}

bool Application::onPrepareFrame(const FrameInfo&, Platform::Error&)
{
    return true;
}

bool Application::onStop(Platform::Error&)
{
    return true;
}
}
