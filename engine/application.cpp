#include "application.h"
#include "frame_clock.h"

#include <chrono>
#include <cmath>
#include <exception>
#include <iterator>
#include <thread>
#include <utility>

namespace Engine
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

bool Application::run(const ApplicationOptions& options, std::vector<Platform::Error>& errors)
{
    errors.clear();
    if (started) {
        errors.push_back(Platform::Error{"Application::run", "Application has already been started.", std::nullopt});
        return false;
    }
    if (!std::isfinite(options.maxDeltaTime) || options.maxDeltaTime <= 0 ||
        !std::isfinite(options.targetFramesPerSecond) || options.targetFramesPerSecond < 0) {
        errors.push_back(Platform::Error{"Application::run", "Invalid frame timing options.", std::nullopt});
        return false;
    }
    started = true;
    if (!Platform::initialize(options.window, errors)) {
        return false;
    }

    // При исключении обработчика onStop() вызывается до уничтожения контекста.
    Platform::Error error;
    bool startAttempted = false;
    try {
        bool ready = true;
        if (options.verticalSync.has_value()) {
            ready = setVSync(*options.verticalSync, error);
        }
        if (ready) {
            startAttempted = true;
            ready = onStart(error);
        }
        if (!ready) {
            errors.push_back(error);
        } else {
            const auto start = Clock::now();
            FrameClock frameClock(options.maxDeltaTime, start);
            auto fpsStart = start;
            double presentedFrames = 0;
            FrameInfo frame;
            bool wasPaused = paused;
            bool wasMinimized = false;
            while (!exitRequested && errors.empty()) {
                const auto frameStart = Clock::now();
                FrameTimings timings;
                if (!Platform::isCloseRequested() && !Platform::processEvents(error)) {
                    errors.push_back(error);
                    break;
                }
                if (!Platform::getWindowState(frame.window, error)) {
                    errors.push_back(error);
                    break;
                }
                frame.realTime = secondsBetween(start, frameStart);
                frame.paused = paused || frame.window.minimized;
                if (!onInput(frame, error)) {
                    errors.push_back(error);
                    break;
                }
                if (exitRequested || Platform::isCloseRequested()) {
                    break;
                }
                auto stageEnd = Clock::now();
                timings.events = secondsBetween(frameStart, stageEnd);

                frame.paused = paused || frame.window.minimized;
                // Первый кадр после продолжения имеет dT=0.
                if (frame.paused || wasPaused) {
                    frameClock.reset(frameStart);
                    frame.deltaTime = 0;
                } else {
                    frame.deltaTime = frameClock.tick(frameStart);
                }
                wasPaused = frame.paused;
                frame.elapsedTime += static_cast<double>(frame.deltaTime);

                if (frame.window.minimized || wasMinimized) {
                    fpsStart = frameStart;
                    presentedFrames = 0;
                    frame.framesPerSecond = 0;
                    frame.fpsUpdated = true;
                }
                wasMinimized = frame.window.minimized;

                const auto updateStart = stageEnd;
                if (!frame.paused && !onUpdate(frame, error)) {
                    errors.push_back(error);
                    break;
                }
                stageEnd = Clock::now();
                timings.update = secondsBetween(updateStart, stageEnd);
                if (exitRequested) {
                    break;
                }

                if (!frame.window.minimized) {
                    const auto prepareStart = stageEnd;
                    if (!onPrepareFrame(frame, error)) {
                        errors.push_back(error);
                        break;
                    }
                    stageEnd = Clock::now();
                    timings.preparation = secondsBetween(prepareStart, stageEnd);
                    if (exitRequested) {
                        break;
                    }
                    const auto renderStart = stageEnd;
                    if (!onRender(frame, error)) {
                        errors.push_back(error);
                        break;
                    }
                    stageEnd = Clock::now();
                    timings.rendering = secondsBetween(renderStart, stageEnd);
                    if (exitRequested) {
                        break;
                    }
                    const auto presentStart = stageEnd;
                    if (!Platform::present(error)) {
                        errors.push_back(error);
                        break;
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
        }
    } catch (const std::exception& exception) {
        errors.push_back(Platform::Error{"Application::run", exception.what(), std::nullopt});
    } catch (...) {
        errors.push_back(Platform::Error{"Application::run", "Unknown exception in application callback.", std::nullopt});
    }

    std::vector<Platform::Error> cleanupErrors;
    try {
        if (startAttempted) {
            onStop(cleanupErrors);
        }
    } catch (const std::exception& exception) {
        cleanupErrors.push_back(Platform::Error{"Application::onStop", exception.what(), std::nullopt});
    } catch (...) {
        cleanupErrors.push_back(Platform::Error{"Application::onStop", "Unknown exception during cleanup.", std::nullopt});
    }
    errors.insert(errors.end(), std::make_move_iterator(cleanupErrors.begin()),
        std::make_move_iterator(cleanupErrors.end()));
    graphics.shutdown();
    Platform::shutdown(cleanupErrors);
    errors.insert(errors.end(), std::make_move_iterator(cleanupErrors.begin()),
        std::make_move_iterator(cleanupErrors.end()));
    return errors.empty();
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

void Application::onStop(std::vector<Platform::Error>&)
{
}
}
