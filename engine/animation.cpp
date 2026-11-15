#include "animation.h"

#include <cmath>
#include <stdexcept>
#include <utility>

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
        if (frame.duration <= 0
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
    frameIndex = frames.empty() || playbackDirection == AnimationDirection::Forward ? 0 : frames.size() - 1;
    if (!frames.empty()) {
        textureRect = frames[frameIndex].textureRect;
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

double Animation::frameStart(std::size_t index) const
{
    double start = 0;
    if (playbackDirection == AnimationDirection::Forward) {
        for (std::size_t position = 0; position < index; ++position) {
            start += frames[position].duration;
        }
    } else {
        for (std::size_t position = frames.size(); position > index + 1; --position) {
            start += frames[position - 1].duration;
        }
    }
    return start;
}

void Animation::setFrame(std::size_t index)
{
    if (index >= frames.size()) {
        throw std::out_of_range("Animation frame index is out of range.");
    }
    elapsed = frameStart(index);
    frameIndex = index;
    textureRect = frames[index].textureRect;
    finished = false;
}

void Animation::setDirection(AnimationDirection value)
{
    if (playbackDirection == value) {
        return;
    }
    const double timeInFrame = frames.empty() || finished ? 0 : elapsed - frameStart(frameIndex);
    playbackDirection = value;
    elapsed = frames.empty() ? 0 : frameStart(frameIndex) + timeInFrame;
    finished = false;
}

void Animation::setSpeed(double value)
{
    if (value < 0) {
        throw std::invalid_argument("Animation speed must be nonnegative.");
    }
    playbackSpeed = value;
}

void Animation::update(double deltaTime)
{
    if (deltaTime < 0) {
        throw std::invalid_argument("Animation deltaTime must be nonnegative.");
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
            frameIndex = playbackDirection == AnimationDirection::Forward ? frames.size() - 1 : 0;
            textureRect = frames[frameIndex].textureRect;
            playing = false;
            finished = true;
            return;
        }
        elapsed += advance;
    } else if (playbackMode == AnimationMode::Clamp) {
        const std::size_t last = playbackDirection == AnimationDirection::Forward ? frames.size() - 1 : 0;
        const double lastStart = frameStart(last);
        if (elapsed >= lastStart || advance >= lastStart - elapsed) {
            const double remaining = elapsed >= lastStart ? 0 : lastStart - elapsed;
            const double local = elapsed >= lastStart ? elapsed - lastStart : 0;
            elapsed = lastStart + std::fmod(local + std::fmod(advance - remaining, frames[last].duration), frames[last].duration);
        } else {
            elapsed += advance;
        }
    } else {
        // Пропускаем полные циклы без перебора кадров каждого прохода.
        const double remainder = std::fmod(advance, duration);
        elapsed = remainder >= duration - elapsed ? remainder - (duration - elapsed) : elapsed + remainder;
    }
    selectFrame();
}

void Animation::selectFrame()
{
    frameIndex = playbackDirection == AnimationDirection::Forward ? 0 : frames.size() - 1;
    double end = frames[frameIndex].duration;
    for (std::size_t visited = 1; visited < frames.size() && elapsed >= end; ++visited) {
        if (playbackDirection == AnimationDirection::Forward) {
            ++frameIndex;
        } else {
            --frameIndex;
        }
        end += frames[frameIndex].duration;
    }
    textureRect = frames[frameIndex].textureRect;
}

bool Animation::load(const TextureAtlas& atlas, std::initializer_list<std::string_view> names, double frameDuration,
    ResourceError& error)
{
    if (names.size() == 0) {
        error = {ResourceErrorCode::InvalidState, "Animation requires at least one atlas region."};
        return false;
    }
    std::vector<Rect> areas;
    areas.reserve(names.size());
    for (const std::string_view name : names) {
        const Rect* area = atlas.find(name);
        if (!area) {
            error = {ResourceErrorCode::InvalidImage, "Atlas region not found: " + std::string(name)};
            return false;
        }
        areas.push_back(*area);
    }
    return load(atlas.texture(), areas, frameDuration, error);
}

bool AnimationSet::load(const TextureAtlas& atlas, const std::vector<Clip>& clips, ResourceError& error)
{
    if (clips.empty()) {
        error = {ResourceErrorCode::InvalidState, "Animation set requires at least one clip."};
        return false;
    }
    std::vector<Entry> loaded;
    loaded.reserve(clips.size());
    for (const Clip& clip : clips) {
        if (clip.name.empty()) {
            error = {ResourceErrorCode::InvalidState, "Animation clip requires a name."};
            return false;
        }
        if (clip.frames.empty()) {
            error = {ResourceErrorCode::InvalidState, "Animation clip " + clip.name + " requires at least one frame."};
            return false;
        }
        std::vector<Rect> areas;
        areas.reserve(clip.frames.size());
        for (const std::string& frame : clip.frames) {
            const Rect* area = atlas.find(frame);
            if (!area) {
                error = {ResourceErrorCode::InvalidImage, "Atlas region not found: " + frame};
                return false;
            }
            areas.push_back(*area);
        }
        Entry entry;
        entry.name = clip.name;
        if (!entry.animation.load(atlas.texture(), areas, clip.frameDuration, error)) {
            return false;
        }
        loaded.push_back(std::move(entry));
    }
    entries = std::move(loaded);
    selected = 0;
    entries.front().animation.restart();
    error = {};
    return true;
}

std::size_t AnimationSet::find(std::string_view name) const
{
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (entries[index].name == name) {
            return index;
        }
    }
    return entries.size();
}

bool AnimationSet::select(std::string_view name)
{
    const std::size_t index = find(name);
    if (index >= entries.size()) {
        return false;
    }
    if (index == selected) {
        return true;
    }
    selected = index;
    entries[selected].animation.restart();
    return true;
}

void AnimationSet::restart()
{
    if (!entries.empty()) {
        entries[selected].animation.restart();
    }
}

void AnimationSet::update(double deltaTime)
{
    if (!entries.empty()) {
        entries[selected].animation.update(deltaTime);
    }
}

void AnimationSet::setFilter(TextureFilter filter)
{
    for (Entry& entry : entries) {
        entry.animation.filter = filter;
    }
}
}
