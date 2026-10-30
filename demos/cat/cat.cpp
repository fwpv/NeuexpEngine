#include "cat.h"
#include "game.h"
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
const Platform* findPlatform(const std::vector<Platform>& platforms, int id)
{
    const auto found = std::find_if(platforms.begin(), platforms.end(), [id](const Platform& platform) {
        return platform.id == id;
    });
    return found == platforms.end() ? nullptr : &*found;
}

constexpr int HeadFrames = Feet - Head;
constexpr int NoPlatform = -1;
constexpr float PositionTolerance = 0.01F; // Сотая пикселя учитывает округление координат float.
constexpr float TimeTolerance = 1e-9F;
constexpr float Gravity = 1500;
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
        retireIgnoredPlatforms(platforms);
        float step = remaining;
        for (auto& platform : platforms) {
            step = platform.prepareMotion(step);
        }
        applySupportCommands(platforms);
        step = move(platforms, step, direction);
        jumpBuffer = std::max(0.0F, jumpBuffer - step);
        remaining -= step;
    }
    animate(duration, direction);
}

void Cat::retireIgnoredPlatforms(const std::vector<Platform>& platforms)
{
    for (auto iterator = ignored.begin(); iterator != ignored.end();) {
        const auto* platform = findPlatform(platforms, *iterator);
        iterator = !platform || feet.y > platform->position.y + platform->size.height ? ignored.erase(iterator) : std::next(iterator);
    }
}

void Cat::applySupportCommands(const std::vector<Platform>& platforms)
{
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
}

std::vector<Cat::Range> Cat::supportRanges(const Platform& platform, const std::vector<Platform>& platforms, Probe probe) const
{
    if (ignored.count(platform.id) != 0) {
        return {};
    }
    const bool continuing = support == platform.id && static_cast<float>(entryDirection) * probe.speed >= 0;
    const bool right = probe.speed > PositionTolerance || (continuing && entryDirection > 0);
    const bool left = probe.speed < -PositionTolerance || (continuing && entryDirection < 0);
    const Range central{platform.position.x - 10.0F - (probe.allowEntry && right ? 10.0F : 0),
        platform.position.x + platform.size.width + 10.0F + (probe.allowEntry && left ? 10.0F : 0)};
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
        const float leftWidth = platform.position.x <= neighbor.position.x ? platform.size.width : neighbor.size.width;
        const float rightWidth = platform.position.x <= neighbor.position.x ? neighbor.size.width : platform.size.width;
        // Границы лап заданы относительно стоп кота; перекрытие с каждой площадкой — хотя бы 3 пикселя.
        constexpr float MinimumPawOverlap = 3;
        constexpr float LeftPawLeft = -60.75F;
        constexpr float LeftPawRight = -20.25F;
        constexpr float RightPawLeft = 8.25F;
        constexpr float RightPawRight = 54.75F;
        constexpr float BridgeMargin = 5;
        Range bridge{std::max(std::initializer_list<float>{leftX + MinimumPawOverlap - LeftPawRight,
            rightX + MinimumPawOverlap - RightPawRight,
            leftX + leftWidth}) - BridgeMargin,
            std::min(std::initializer_list<float>{leftX + leftWidth - MinimumPawOverlap - LeftPawLeft,
                rightX + rightWidth - MinimumPawOverlap - RightPawLeft, rightX}) + BridgeMargin};
        if (leftX + leftWidth >= rightX || bridge.left > bridge.right) {
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
    ranges.resize(merged);
    return ranges;
}

std::optional<Cat::Range> Cat::supportRange(const Platform& platform, const std::vector<Platform>& platforms, Probe probe) const
{
    const auto ranges = supportRanges(platform, platforms, probe);
    std::optional<Range> ahead;
    for (const auto range : ranges) {
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
    const int direction = x > platform.position.x + platform.size.width / 2.0F ? 1 : -1;
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

namespace
{
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

Engine::Vector2 Cat::Motion::positionAt(float time) const
{
    return origin + velocity * time + acceleration * (time * time / 2);
}

Cat::Motion Cat::planMotion(float duration, int direction, const Platform* standing, bool entering) const
{
    constexpr float RunningSpeed = 600;
    constexpr float GroundAcceleration = 2400;
    constexpr float AirAcceleration = 1200;
    constexpr float GroundBraking = 4800;
    constexpr float AirBraking = 800;
    const float target = direction != 0 ? static_cast<float>(direction) * RunningSpeed : (entering ? velocity.x : 0);
    const float acceleration = direction == 0
        ? (standing ? GroundBraking : AirBraking) : (standing ? GroundAcceleration : AirAcceleration);
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
    return Motion{feet, velocity, accelerationVector, target, targetTime, duration};
}

void Cat::limitSupportedMotion(const std::vector<Platform>& platforms, const Platform& standing,
    bool entering, const Motion& motion, Contact& contact) const
{
    if (entering && static_cast<float>(entryDirection) * velocity.x <= PositionTolerance && static_cast<float>(entryDirection) * motion.acceleration.x < 0) {
        contact.time = 0;
        contact.kind = Contact::Kind::Departure;
    }
    // Мост может исчезнуть раньше достижения горизонтального края.
    constexpr float WidestBridge = 119.5F;
    for (const auto& neighbor : platforms) {
        const float leftWidth = neighbor.position.x < standing.position.x ? neighbor.size.width : standing.size.width;
        const float gap = std::abs(neighbor.position.x - standing.position.x) - leftWidth;
        const float relativeSpeed = neighbor.speed - standing.speed;
        if (neighbor.id == support || ignored.count(neighbor.id) != 0
            || gap <= 0 || gap > WidestBridge || relativeSpeed == 0) {
            continue;
        }
        for (const float height : std::initializer_list<float>{-5.0F, 5.0F}) {
            const float time = (height - (neighbor.position.y - standing.position.y)) / relativeSpeed;
            if (time > TimeTolerance) {
                contact.time = std::min(contact.time, time);
            }
        }
    }
    float destination = motion.positionAt(contact.time).x;
    if (entering) {
        const float boundary = standing.position.x + (entryDirection > 0 ? -10.0F : standing.size.width + 10.0F);
        const auto settled = *supportRange(standing, platforms, Probe{boundary, 0, 0, false});
        const float entryX = entryDirection > 0 ? settled.left : settled.right;
        if ((entryDirection > 0 && destination >= entryX) || (entryDirection < 0 && destination <= entryX)) {
            contact.time = std::clamp(travelTime(velocity.x, motion.acceleration.x, entryX - feet.x), 0.0F, contact.time);
            destination = motion.positionAt(contact.time).x;
        }
    }
    if (const auto range = *supportRange(standing, platforms, Probe{feet.x, velocity.x});
        destination > range.right + PositionTolerance || destination < range.left - PositionTolerance) {
        contact.edgeX = destination > feet.x ? range.right : range.left;
        contact.time = std::clamp(travelTime(velocity.x, motion.acceleration.x, contact.edgeX - feet.x), 0.0F, contact.time);
        contact.kind = Contact::Kind::Departure;
    }
}

void Cat::findAirborneContact(const std::vector<Platform>& platforms, const Motion& motion, Contact& contact) const
{
    for (const auto& platform : platforms) {
        if (ignored.count(platform.id) != 0) {
            continue;
        }
        const auto land = [&](float time) {
            contact.time = std::max(0.0F, time);
            contact.kind = Contact::Kind::Landing;
            contact.platform = &platform;
        };
        // Контакт сверху проверяется первым; равное время сохраняет прежнего кандидата.
        const float gap = feet.y - platform.position.y;
        const float topTime = landingTime(gap, velocity.y - platform.speed);
        if (gap <= PositionTolerance && topTime >= -TimeTolerance && topTime <= contact.time + TimeTolerance
            && supportRange(platform, platforms,
                Probe{motion.positionAt(topTime).x, velocity.x + motion.acceleration.x * topTime, topTime})
            && (!contact.platform || topTime < contact.time - TimeTolerance)) {
            land(topTime);
        }
        // Боковой въезд допускается только к центру опоры и без подъёма относительно неё.
        if (std::abs(velocity.x) <= PositionTolerance) {
            continue;
        }
        const auto approach = supportRange(platform, platforms, Probe{feet.x, velocity.x, 0, true, true});
        if (!approach) {
            continue;
        }
        const float entryX = std::clamp(feet.x, approach->left, approach->right);
        const float destination = motion.positionAt(contact.time).x;
        if ((velocity.x > 0 && destination < entryX) || (velocity.x < 0 && destination > entryX)) {
            continue;
        }
        const float time = std::abs(entryX - feet.x) <= PositionTolerance
            ? 0 : travelTime(velocity.x, motion.acceleration.x, entryX - feet.x);
        const Engine::Vector2 speed = velocity + motion.acceleration * time;
        if (time < 0 || time > contact.time || (contact.platform && time >= contact.time - TimeTolerance)
            || (platform.position.x + platform.size.width / 2.0F - entryX) * speed.x <= PositionTolerance
            || speed.y - platform.speed < -PositionTolerance
            || !supportRange(platform, platforms, Probe{entryX, speed.x, time})) {
            continue;
        }
        const bool bridging = (entryX < platform.position.x - 10.0F
            || entryX > platform.position.x + platform.size.width + 10.0F)
            && supportRange(platform, platforms, Probe{entryX, 0, time, false});
        const float clearance = motion.positionAt(time).y - (platform.position.y + platform.speed * time);
        if (clearance >= -PositionTolerance && clearance <= 5.0F * (bridging ? 2 : 1) + PositionTolerance) {
            land(time);
        }
    }
    const float exitTime = landingTime(feet.y - (700 + 123.75F), velocity.y);
    if (exitTime <= contact.time && (!contact.platform || exitTime < contact.time - TimeTolerance)) {
        contact.time = exitTime;
        contact.kind = Contact::Kind::ReturnToTop;
    }
}

float Cat::move(std::vector<Platform>& platforms, float duration, int direction)
{
    const Platform* standing = findPlatform(platforms, support);
    const bool entering = standing && !supportRange(*standing, platforms, Probe{feet.x, 0, 0, false});
    if (!entering) {
        entryDirection = 0;
    } else if (entryDirection == 0) {
        entryDirection = velocity.x > 0 ? 1 : -1;
    }
    const Motion motion = planMotion(duration, direction, standing, entering);
    Contact contact{motion.duration, Contact::Kind::None, nullptr, feet.x};
    // Ограничения выбираются до интеграции: кот и лифты используют одно время контакта.
    if (standing) {
        limitSupportedMotion(platforms, *standing, entering, motion, contact);
    } else {
        findAirborneContact(platforms, motion, contact);
    }
    feet = motion.positionAt(contact.time);
    velocity += motion.acceleration * contact.time;
    if (motion.acceleration.x != 0 && std::abs(contact.time - motion.targetTime) <= TimeTolerance) {
        velocity.x = motion.targetSpeed;
    }
    for (auto& platform : platforms) {
        platform.position.y += platform.speed * contact.time;
    }
    if (contact.kind == Contact::Kind::Landing) {
        support = contact.platform->id;
        entryDirection = 0;
        ignored.clear();
    }
    if (const auto* platform = findPlatform(platforms, support)) {
        feet.y = platform->position.y;
        velocity.y = platform->speed;
        if (!supportRange(*platform, platforms, Probe{feet.x, velocity.x})) {
            leaveEdge(*platform, platforms, feet.x);
        } else if (contact.kind == Contact::Kind::Departure) {
            leaveEdge(*platform, platforms, contact.edgeX);
        }
    }
    if (contact.kind == Contact::Kind::ReturnToTop) {
        returnToTop();
    }
    return contact.time;
}

void Cat::draw(Engine::Renderer2D& renderer, const Images& images) const
{
    const Engine::Vector2 origin = feet + Engine::Vector2{-101.25F, -123.75F + 3 + (support >= 0 ? 2 : 0)};
    images.draw(renderer, Head + static_cast<std::size_t>(headFrame), origin, 0.75F);
    images.draw(renderer, Feet + static_cast<std::size_t>(feetFrame),
        origin + Engine::Vector2{44 * 0.75F, 120 * 0.75F}, 0.75F);
}

}
