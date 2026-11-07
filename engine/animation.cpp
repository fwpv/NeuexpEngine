#include "animation.h"

#include <cmath>
#include <stdexcept>

namespace Engine
{
bool Animation::load(const Texture& source, const std::vector<AnimationFrame>& sequence, ResourceError& error)
{
    if (!source.isValid() || sequence.empty()) {
        error = {ResourceErrorCode::InvalidState, "Animation requires a valid texture and at least one frame."};
        return false;
    }
    double total = 0;
    for (const auto& frame : sequence) {
        const Rect& area = frame.textureRect;
        if (!std::isfinite(frame.duration) || frame.duration <= 0
            || !std::isfinite(area.position.x) || !std::isfinite(area.position.y)
            || !std::isfinite(area.size.width) || !std::isfinite(area.size.height)
            || area.position.x < 0 || area.position.y < 0 || area.size.width <= 0 || area.size.height <= 0
            || area.size != sequence.front().textureRect.size
            || static_cast<double>(area.position.x) + area.size.width > source.width()
            || static_cast<double>(area.position.y) + area.size.height > source.height()
            || !std::isfinite(total + frame.duration) || total + frame.duration <= total) {
            error = {ResourceErrorCode::InvalidState, "Invalid animation frame rectangle or duration."};
            return false;
        }
        total += frame.duration;
    }
    frames = sequence;
    texture = source;
    size = frames.front().textureRect.size;
    duration = total;
    playbackSpeed = 1;
    stop();
    error = {};
    return true;
}

bool Animation::load(const Texture& source, const std::vector<Rect>& areas, double frameDuration, ResourceError& error)
{
    std::vector<AnimationFrame> sequence;
    sequence.reserve(areas.size());
    for (const Rect& area : areas) {
        sequence.push_back(AnimationFrame{area, frameDuration});
    }
    return load(source, sequence, error);
}

void Animation::play()
{
    playing = !frames.empty() && !finished;
}

void Animation::pause()
{
    playing = false;
}

void Animation::stop()
{
    playing = false;
    finished = false;
    elapsed = 0;
    frameIndex = 0;
    if (!frames.empty()) {
        textureRect = frames.front().textureRect;
    }
}

void Animation::restart()
{
    stop();
    play();
}

void Animation::setMode(AnimationMode value)
{
    playbackMode = value;
}

void Animation::setSpeed(double value)
{
    if (!std::isfinite(value) || value < 0) {
        throw std::invalid_argument("Animation speed must be finite and nonnegative.");
    }
    playbackSpeed = value;
}

void Animation::update(double deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime < 0) {
        throw std::invalid_argument("Animation deltaTime must be finite and nonnegative.");
    }
    if (!playing || deltaTime == 0 || playbackSpeed == 0) {
        return;
    }
    const double advance = deltaTime * playbackSpeed;
    if (!std::isfinite(advance)) {
        throw std::invalid_argument("Animation time advance overflow.");
    }
    if (playbackMode == AnimationMode::Once) {
        if (advance >= duration - elapsed) {
            elapsed = duration;
            frameIndex = frames.size() - 1;
            textureRect = frames.back().textureRect;
            playing = false;
            finished = true;
            return;
        }
        elapsed += advance;
    } else {
        // Пропускаем полные циклы без перебора кадров каждого прохода.
        const double remainder = std::fmod(advance, duration);
        elapsed = remainder >= duration - elapsed ? remainder - (duration - elapsed) : elapsed + remainder;
    }
    selectFrame();
}

void Animation::selectFrame()
{
    double end = frames.front().duration;
    frameIndex = 0;
    while (frameIndex + 1 < frames.size() && elapsed >= end) {
        ++frameIndex;
        end += frames[frameIndex].duration;
    }
    textureRect = frames[frameIndex].textureRect;
}
}
