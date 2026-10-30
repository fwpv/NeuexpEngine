#include "world.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <vector>

namespace Cat
{
namespace
{
constexpr int NoPlatform = -1;

constexpr int HeadFrames = Feet - Head;
constexpr float PositionTolerance = 0.01F; // Сотая пикселя учитывает округление координат float.
constexpr float TimeTolerance = 1e-9F;
constexpr float Gravity = 1500;

const Platform* findPlatform(const std::vector<Platform>& platforms, int id)
{
    const auto found = std::find_if(platforms.begin(), platforms.end(), [id](const Platform& platform) {
        return platform.id == id;
    });
    return found == platforms.end() ? nullptr : &*found;
}

// Время до координаты на монотонном участке движения.
float travelTime(float speed, float acceleration, float distance)
{
    if (acceleration == 0) {
        return distance / speed;
    }
    const float root = std::sqrt(std::max(0.0F, speed * speed + 2 * acceleration * distance));
    const float first = (-speed + root) / acceleration;
    const float second = (-speed - root) / acceleration;
    float result = first >= -TimeTolerance ? first : second;
    if (second >= -TimeTolerance) {
        result = std::min(result, second);
    }
    return result;
}

// Встреча с поверхностью при движении вниз относительно неё: gap <= 0.
float landingTime(float gap, float speed)
{
    const float root = std::sqrt(std::max(0.0F, speed * speed - 2 * Gravity * gap));
    return speed >= 0 && root + speed > 0 ? -2 * gap / (root + speed) : (root - speed) / Gravity;
}
}

Cat::Probe::Probe(float x, float speed, float after, bool allowEntry, bool ahead)
    : x(x)
    , speed(speed)
    , after(after)
    , allowEntry(allowEntry)
    , ahead(ahead)
{
}

Engine::Vector2 Cat::position() const
{
    return feet;
}

bool Cat::standsOn(const Platform& platform) const
{
    return support == platform.id;
}

float Platform::prepareMotion(float duration)
{
    if (speed == 0) {
        return duration;
    }
    if (position.y <= top + PositionTolerance) {
        position.y = top;
        speed = std::abs(speed);
    } else if (position.y >= bottom - PositionTolerance) {
        position.y = bottom;
        speed = -std::abs(speed);
    }
    return std::min(duration, (speed > 0 ? bottom - position.y : position.y - top) / std::abs(speed));
}

void Cat::animate(float duration, int direction)
{
    constexpr float AnimationFrameDuration = 0.03F;
    animationTime += duration;
    const int steps = direction == 0 ? 0 : static_cast<int>((animationTime - frameTime) / AnimationFrameDuration);
    headFrame = std::clamp(headFrame + direction * steps, 0, HeadFrames - 1);
    constexpr int FeetFrames = ArtCount - Feet;
    feetFrame = ((feetFrame + direction * (steps % FeetFrames)) % FeetFrames + FeetFrames) % FeetFrames;
    frameTime = direction == 0 ? animationTime : frameTime + static_cast<float>(steps) * AnimationFrameDuration;
}

void Cat::returnToTop()
{
    support = NoPlatform;
    entryDirection = 0;
    ignored.clear();
    feet.y = 0;
    velocity.y = 500;
    jumpBuffer = 0;
}

void Cat::advance(std::vector<Platform>& platforms, float duration, const Controls& controls)
{
    const int direction = static_cast<int>(controls.right) - static_cast<int>(controls.left);
    if (controls.jump) {
        jumpBuffer = 0.1F;
    }
    dropRequested = dropRequested || controls.drop;
    for (float remaining = duration; remaining > TimeTolerance;) {
        if (feet.y >= 700 + 123.75F) {
            returnToTop();
        }
        for (auto iterator = ignored.begin(); iterator != ignored.end();) {
            const auto* platform = findPlatform(platforms, *iterator);
            iterator = !platform || feet.y > platform->position.y + 35 ? ignored.erase(iterator) : std::next(iterator);
        }
        float step = remaining;
        for (auto& platform : platforms) {
            step = platform.prepareMotion(step);
        }
        const Platform* standing = findPlatform(platforms, support);
        if (standing && !supportRange(*standing, platforms, Probe{feet.x, velocity.x})) {
            const int previous = support;
            const float speed = standing->speed;
            standing = findSupport(platforms, 0, previous);
            support = standing ? standing->id : NoPlatform;
            if (!standing) {
                velocity.y = speed;
                ignored.insert(previous);
                entryDirection = 0;
            }
        }
        if (standing) {
            feet.y = standing->position.y;
            velocity.y = standing->speed;
            if (dropRequested || jumpBuffer > 0) {
                if (dropRequested) {
                    ignoreSupports(platforms);
                }
                velocity.y = dropRequested ? std::max(200.0F, velocity.y + 200) : velocity.y - 810;
                support = NoPlatform;
                entryDirection = 0;
                jumpBuffer = 0;
            }
        }
        dropRequested = false;
        step = move(platforms, step, direction);
        jumpBuffer = std::max(0.0F, jumpBuffer - step);
        remaining -= step;
    }
    animate(duration, direction);
}

std::optional<Cat::Range> Cat::supportRange(const Platform& platform, const std::vector<Platform>& platforms, Probe probe) const
{
    if (ignored.count(platform.id) != 0) {
        return std::nullopt;
    }
    const bool continuing = support == platform.id && static_cast<float>(entryDirection) * probe.speed >= 0;
    const bool right = probe.speed > PositionTolerance || (continuing && entryDirection > 0);
    const bool left = probe.speed < -PositionTolerance || (continuing && entryDirection < 0);
    const Range central{platform.position.x - 10.0F - (probe.allowEntry && right ? 10.0F : 0),
        platform.position.x + 175 + 10.0F + (probe.allowEntry && left ? 10.0F : 0)};
    std::vector<Range> ranges;
    ranges.reserve(platforms.size() + 1);
    ranges.push_back(central);
    for (const auto& neighbor : platforms) {
        const float relativeSpeed = neighbor.speed - platform.speed;
        const float height = neighbor.position.y - platform.position.y + relativeSpeed * probe.after;
        if (neighbor.id == platform.id || std::abs(height) > 5.0F + PositionTolerance
            || (std::abs(height) >= 5.0F - PositionTolerance && height * relativeSpeed > 0)) {
            continue;
        }
        const float leftX = std::min(platform.position.x, neighbor.position.x);
        const float rightX = std::max(platform.position.x, neighbor.position.x);
        // Каждая лапа должна перекрывать свою площадку хотя бы на 3 пикселя.
        Range bridge{std::max(std::initializer_list<float>{leftX + 3.0F + 20.25F, rightX + 3.0F - 54.75F,
            leftX + 175}) - 5.0F,
            std::min(std::initializer_list<float>{leftX + 175 - 3.0F + 60.75F,
                rightX + 175 - 3.0F - 8.25F, rightX}) + 5.0F};
        if (leftX + 175 >= rightX || bridge.left > bridge.right) {
            continue;
        }
        if (probe.allowEntry && right && bridge.right < central.left) {
            bridge.right = central.left;
        } else if (probe.allowEntry && left && bridge.left > central.right) {
            bridge.left = central.right;
        }
        ranges.push_back(bridge);
    }
    std::sort(ranges.begin(), ranges.end(), [](Range a, Range b) {
        return a.left < b.left;
    });
    std::size_t merged = 0;
    for (std::size_t i = 0; i < ranges.size(); ++i) {
        if (merged > 0 && ranges[i].left <= ranges[merged - 1].right + PositionTolerance) {
            ranges[merged - 1].right = std::max(ranges[merged - 1].right, ranges[i].right);
        } else {
            ranges[merged++] = ranges[i];
        }
    }
    std::optional<Range> ahead;
    for (std::size_t i = 0; i < merged; ++i) {
        const auto range = ranges[i];
        if (probe.x >= range.left - PositionTolerance && probe.x <= range.right + PositionTolerance) {
            return range;
        }
        if (probe.ahead && ((probe.speed > 0 && range.left > probe.x && !ahead) || (probe.speed < 0 && range.right < probe.x))) {
            ahead = range;
        }
    }
    return ahead;
}

const Platform* Cat::findSupport(const std::vector<Platform>& platforms, int direction, int excluded) const
{
    const Platform* best = nullptr;
    for (const auto& platform : platforms) {
        const float distance = std::abs(platform.position.y - feet.y);
        if (platform.id == excluded || distance > 5.0F + PositionTolerance) {
            continue;
        }
        const auto range = supportRange(platform, platforms, Probe{feet.x, velocity.x});
        if (range && !(direction > 0 && range->right <= feet.x + PositionTolerance)
            && !(direction < 0 && range->left >= feet.x - PositionTolerance)
            && (!best || distance < std::abs(best->position.y - feet.y) - PositionTolerance)) {
            best = &platform;
        }
    }
    return best;
}

void Cat::ignoreSupports(const std::vector<Platform>& platforms)
{
    for (const auto& platform : platforms) {
        if (std::abs(platform.position.y - feet.y) <= 5.0F + PositionTolerance
            && supportRange(platform, platforms, Probe{feet.x, velocity.x})) {
            ignored.insert(platform.id);
        }
    }
}

void Cat::leaveEdge(const Platform& platform, const std::vector<Platform>& platforms, float x)
{
    feet.x = x;
    const int direction = x > platform.position.x + 175 / 2.0F ? 1 : -1;
    if (const auto* next = findSupport(platforms, direction, platform.id)) {
        support = next->id;
        feet.y = next->position.y;
        velocity.y = next->speed;
    } else {
        velocity.y = platform.speed;
        ignoreSupports(platforms);
        ignored.insert(platform.id);
        support = NoPlatform;
    }
    entryDirection = 0;
}

float Cat::move(std::vector<Platform>& platforms, float duration, int direction)
{
    // Участок заканчивается на ближайшем контакте, крае, развороте лифта или изменении ускорения.
    enum class Event
    {
        None,
        Landing,
        Departure,
        ReturnToTop
    };
    const Platform* standing = findPlatform(platforms, support);
    const bool entering = standing && !supportRange(*standing, platforms, Probe{feet.x, 0, 0, false});
    if (!entering) {
        entryDirection = 0;
    } else if (entryDirection == 0) {
        entryDirection = velocity.x > 0 ? 1 : -1;
    }
    const float target = direction != 0 ? static_cast<float>(direction) * 600 : (entering ? velocity.x : 0);
    const float acceleration = direction == 0
        ? (standing ? 4800.0F : 800.0F) : (standing ? 2400.0F : 1200.0F);
    const float difference = target - velocity.x;
    const Engine::Vector2 accelerationVector{std::abs(difference) < PositionTolerance ? 0 : std::copysign(acceleration, difference),
        standing ? 0 : Gravity};
    const float targetTime = std::abs(difference) / acceleration;
    if (accelerationVector.x != 0) {
        duration = std::min(duration, targetTime);
        if (velocity.x * accelerationVector.x < 0) {
            duration = std::min(duration, -velocity.x / accelerationVector.x);
        }
    }
    const auto positionAt = [&](float t) {
        return feet + velocity * t + accelerationVector * (t * t / 2);
    };
    Event event = Event::None;
    const Platform* landing = nullptr;
    float edgeX = feet.x;
    if (standing) {
        if (entering && static_cast<float>(entryDirection) * velocity.x <= PositionTolerance && static_cast<float>(entryDirection) * accelerationVector.x < 0) {
            duration = 0;
            event = Event::Departure;
        }
        constexpr float WidestBridge = 119.5F;
        for (const auto& neighbor : platforms) {
            const float gap = std::abs(neighbor.position.x - standing->position.x) - 175;
            const float relativeSpeed = neighbor.speed - standing->speed;
            if (neighbor.id == support || ignored.count(neighbor.id) != 0
                || gap <= 0 || gap > WidestBridge || relativeSpeed == 0) {
                continue;
            }
            for (const float height : std::initializer_list<float>{-5.0F, 5.0F}) {
                const float time = (height - (neighbor.position.y - standing->position.y)) / relativeSpeed;
                if (time > TimeTolerance) {
                    duration = std::min(duration, time);
                }
            }
        }
        float destination = positionAt(duration).x;
        if (entering) {
            const float boundary = standing->position.x + (entryDirection > 0 ? -10.0F : 175 + 10.0F);
            const auto settled = *supportRange(*standing, platforms, Probe{boundary, 0, 0, false});
            const float entryX = entryDirection > 0 ? settled.left : settled.right;
            if ((entryDirection > 0 && destination >= entryX) || (entryDirection < 0 && destination <= entryX)) {
                duration = std::clamp(travelTime(velocity.x, accelerationVector.x, entryX - feet.x), 0.0F, duration);
                destination = positionAt(duration).x;
            }
        }
        if (const auto range = *supportRange(*standing, platforms, Probe{feet.x, velocity.x});
            destination > range.right + PositionTolerance || destination < range.left - PositionTolerance) {
            edgeX = destination > feet.x ? range.right : range.left;
            duration = std::clamp(travelTime(velocity.x, accelerationVector.x, edgeX - feet.x), 0.0F, duration);
            event = Event::Departure;
        }
    } else {
        for (const auto& platform : platforms) {
            if (ignored.count(platform.id) != 0) {
                continue;
            }
            const auto land = [&](float time) {
                duration = std::max(0.0F, time);
                event = Event::Landing;
                landing = &platform;
            };
            const float gap = feet.y - platform.position.y;
            const float topTime = landingTime(gap, velocity.y - platform.speed);
            if (gap <= PositionTolerance && topTime >= -TimeTolerance && topTime <= duration + TimeTolerance
                && supportRange(platform, platforms,
                    Probe{positionAt(topTime).x, velocity.x + accelerationVector.x * topTime, topTime})
                && (!landing || topTime < duration - TimeTolerance)) {
                land(topTime);
            }
            if (std::abs(velocity.x) <= PositionTolerance) {
                continue;
            }
            const auto approach = supportRange(platform, platforms, Probe{feet.x, velocity.x, 0, true, true});
            if (!approach) {
                continue;
            }
            const float entryX = std::clamp(feet.x, approach->left, approach->right);
            const float destination = positionAt(duration).x;
            if ((velocity.x > 0 && destination < entryX) || (velocity.x < 0 && destination > entryX)) {
                continue;
            }
            const float time = std::abs(entryX - feet.x) <= PositionTolerance
                ? 0 : travelTime(velocity.x, accelerationVector.x, entryX - feet.x);
            const Engine::Vector2 speed = velocity + accelerationVector * time;
            if (time < 0 || time > duration || (landing && time >= duration - TimeTolerance)
                || (platform.position.x + 175 / 2.0F - entryX) * speed.x <= PositionTolerance
                || speed.y - platform.speed < -PositionTolerance
                || !supportRange(platform, platforms, Probe{entryX, speed.x, time})) {
                continue;
            }
            const bool bridging = (entryX < platform.position.x - 10.0F
                || entryX > platform.position.x + 175 + 10.0F)
                && supportRange(platform, platforms, Probe{entryX, 0, time, false});
            const float clearance = positionAt(time).y - (platform.position.y + platform.speed * time);
            if (clearance >= -PositionTolerance && clearance <= 5.0F * (bridging ? 2 : 1) + PositionTolerance) {
                land(time);
            }
        }
        const float exitTime = landingTime(feet.y - (700 + 123.75F), velocity.y);
        if (exitTime <= duration && (!landing || exitTime < duration - TimeTolerance)) {
            duration = exitTime;
            event = Event::ReturnToTop;
        }
    }
    feet = positionAt(duration);
    velocity += accelerationVector * duration;
    if (accelerationVector.x != 0 && std::abs(duration - targetTime) <= TimeTolerance) {
        velocity.x = target;
    }
    for (auto& platform : platforms) {
        platform.position.y += platform.speed * duration;
    }
    if (event == Event::Landing) {
        support = landing->id;
        entryDirection = 0;
        ignored.clear();
    }
    if (const auto* platform = findPlatform(platforms, support)) {
        feet.y = platform->position.y;
        velocity.y = platform->speed;
        if (!supportRange(*platform, platforms, Probe{feet.x, velocity.x})) {
            leaveEdge(*platform, platforms, feet.x);
        } else if (event == Event::Departure) {
            leaveEdge(*platform, platforms, edgeX);
        }
    }
    if (event == Event::ReturnToTop) {
        returnToTop();
    }
    return duration;
}
}
