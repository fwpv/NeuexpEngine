#include "physics.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <optional>
#include <vector>

namespace Cat
{
namespace
{
const Surface* findSurface(const std::vector<Surface*>& surfaces, int id)
{
    const auto found = std::find_if(surfaces.begin(), surfaces.end(), [id](const Surface* surface) {
        return surface->id == id;
    });
    return found == surfaces.end() ? nullptr : *found;
}

constexpr int NoSurface = -1;
constexpr float PositionTolerance = 0.01F; // Сотая пикселя учитывает округление координат float.
constexpr float TimeTolerance = CatPhysics::TimeTolerance;
}

CatPhysics::CatPhysics(Body& body, std::vector<Surface*>& surfaces)
    : body(body)
    , surfaces(surfaces)
{
}

bool CatPhysics::supports(int surfaceId) const
{
    return contactState.surfaceId == surfaceId;
}

bool CatPhysics::hasSupport() const
{
    return contactState.surfaceId >= 0;
}

bool CatPhysics::isCapturingSupport() const
{
    const auto* surface = findSurface(surfaces, contactState.surfaceId);
    return surface && !supportInterval(*surface, body.position.x, SupportQuery{0, 0, false});
}

void CatPhysics::clearContacts()
{
    contactState.surfaceId = NoSurface;
    contactState.entrySide = 0;
    contactState.excluded.clear();
}

void CatPhysics::detachSupport(bool excludeContacts)
{
    if (excludeContacts) {
        excludeContactSurfaces();
    }
    contactState.surfaceId = NoSurface;
    contactState.entrySide = 0;
}

float CatPhysics::prepareSurfaceMotion(Surface& surface, float duration)
{
    constexpr float TurnTolerance = 0.01F;
    if (surface.speed == 0) {
        return duration;
    }
    if (surface.position.y <= surface.top + TurnTolerance) {
        surface.position.y = surface.top;
        surface.speed = std::abs(surface.speed);
    } else if (surface.position.y >= surface.bottom - TurnTolerance) {
        surface.position.y = surface.bottom;
        surface.speed = -std::abs(surface.speed);
    }
    return std::min(duration, (surface.speed > 0 ? surface.bottom - surface.position.y : surface.position.y - surface.top) / std::abs(surface.speed));
}

float CatPhysics::prepareStep(float duration)
{
    retireExcludedSurfaces();
    for (auto* surface : surfaces) {
        duration = prepareSurfaceMotion(*surface, duration);
    }
    refreshSupport();
    return duration;
}

void CatPhysics::retireExcludedSurfaces()
{
    for (auto iterator = contactState.excluded.begin(); iterator != contactState.excluded.end();) {
        const auto* surface = findSurface(surfaces, *iterator);
        iterator = !surface || body.position.y > surface->position.y + surface->size.height ? contactState.excluded.erase(iterator) : std::next(iterator);
    }
}

void CatPhysics::refreshSupport()
{
    const Surface* standing = findSurface(surfaces, contactState.surfaceId);
    if (standing && !supportInterval(*standing, body.position.x, SupportQuery{body.velocity.x, 0, true})) {
        const int previous = contactState.surfaceId;
        const float speed = standing->speed;
        standing = findSupport(0, previous);
        contactState.surfaceId = standing ? standing->id : NoSurface;
        if (!standing) {
            body.velocity.y = speed;
            contactState.excluded.insert(previous);
            contactState.entrySide = 0;
        }
    }
    if (standing) {
        body.position.y = standing->position.y;
        body.velocity.y = standing->speed;
    }
}

std::vector<Interval> CatPhysics::supportIntervals(const Surface& surface, SupportQuery query) const
{
    if (contactState.excluded.count(surface.id) != 0) {
        return {};
    }
    const bool continuing = contactState.surfaceId == surface.id && static_cast<float>(contactState.entrySide) * query.speed >= 0;
    const bool right = query.speed > PositionTolerance || (continuing && contactState.entrySide > 0);
    const bool left = query.speed < -PositionTolerance || (continuing && contactState.entrySide < 0);
    const Interval central{surface.position.x - body.supportShape.overhang - (query.allowCapture && right ? body.supportShape.captureDistance : 0),
        surface.position.x + surface.size.width + body.supportShape.overhang + (query.allowCapture && left ? body.supportShape.captureDistance : 0)};
    std::vector<Interval> ranges;
    ranges.reserve(surfaces.size() + 1);
    ranges.push_back(central);
    for (const auto* neighborPointer : surfaces) {
        const auto& neighbor = *neighborPointer;
        const float relativeSpeed = neighbor.speed - surface.speed;
        const float height = neighbor.position.y - surface.position.y + relativeSpeed * query.time;
        if (neighbor.id == surface.id || std::abs(height) > body.supportShape.heightAllowance + PositionTolerance
            || (std::abs(height) >= body.supportShape.heightAllowance - PositionTolerance && height * relativeSpeed > 0)) {
            continue;
        }
        const float leftX = std::min(surface.position.x, neighbor.position.x);
        const float rightX = std::max(surface.position.x, neighbor.position.x);
        const float leftWidth = surface.position.x <= neighbor.position.x ? surface.size.width : neighbor.size.width;
        const float rightWidth = surface.position.x <= neighbor.position.x ? neighbor.size.width : surface.size.width;
        // Каждый опорный отрезок перекрывает свою поверхность на заданную величину minimumOverlap.
        Interval bridge{std::max(std::initializer_list<float>{leftX + body.supportShape.minimumOverlap - body.supportShape.segments[0].right,
            rightX + body.supportShape.minimumOverlap - body.supportShape.segments[1].right,
            leftX + leftWidth}) - body.supportShape.bridgeMargin,
            std::min(std::initializer_list<float>{leftX + leftWidth - body.supportShape.minimumOverlap - body.supportShape.segments[0].left,
                rightX + rightWidth - body.supportShape.minimumOverlap - body.supportShape.segments[1].left, rightX}) + body.supportShape.bridgeMargin};
        if (leftX + leftWidth >= rightX || bridge.left > bridge.right) {
            continue;
        }
        if (query.allowCapture && right && bridge.right < central.left) {
            bridge.right = central.left;
        } else if (query.allowCapture && left && bridge.left > central.right) {
            bridge.left = central.right;
        }
        ranges.push_back(bridge);
    }
    std::sort(ranges.begin(), ranges.end(), [](Interval a, Interval b) {
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

std::optional<Interval> CatPhysics::supportInterval(const Surface& surface, float x, SupportQuery query) const
{
    for (const auto interval : supportIntervals(surface, query)) {
        if (x >= interval.left - PositionTolerance && x <= interval.right + PositionTolerance) {
            return interval;
        }
    }
    return std::nullopt;
}

std::optional<Interval> CatPhysics::nextSupportInterval(const Surface& surface, float x, SupportQuery query) const
{
    std::optional<Interval> next;
    for (const auto interval : supportIntervals(surface, query)) {
        if (x >= interval.left - PositionTolerance && x <= interval.right + PositionTolerance) {
            return interval;
        }
        if ((query.speed > 0 && interval.left > x && !next) || (query.speed < 0 && interval.right < x)) {
            next = interval;
        }
    }
    return next;
}

const Surface* CatPhysics::findSupport(int direction, int excluded) const
{
    const Surface* best = nullptr;
    for (const auto* surfacePointer : surfaces) {
        const auto& surface = *surfacePointer;
        const float distance = std::abs(surface.position.y - body.position.y);
        if (surface.id == excluded || distance > body.supportShape.heightAllowance + PositionTolerance) {
            continue;
        }
        const auto range = supportInterval(surface, body.position.x, SupportQuery{body.velocity.x, 0, true});
        if (range && !(direction > 0 && range->right <= body.position.x + PositionTolerance)
            && !(direction < 0 && range->left >= body.position.x - PositionTolerance)
            && (!best || distance < std::abs(best->position.y - body.position.y) - PositionTolerance)) {
            best = &surface;
        }
    }
    return best;
}

void CatPhysics::excludeContactSurfaces()
{
    for (const auto* surfacePointer : surfaces) {
        const auto& surface = *surfacePointer;
        if (std::abs(surface.position.y - body.position.y) <= body.supportShape.heightAllowance + PositionTolerance
            && supportInterval(surface, body.position.x, SupportQuery{body.velocity.x, 0, true})) {
            contactState.excluded.insert(surface.id);
        }
    }
}

void CatPhysics::leaveEdge(const Surface& surface, float x)
{
    body.position.x = x;
    const int direction = x > surface.position.x + surface.size.width / 2.0F ? 1 : -1;
    if (const auto* next = findSupport(direction, surface.id)) {
        contactState.surfaceId = next->id;
        body.position.y = next->position.y;
        body.velocity.y = next->speed;
    } else {
        body.velocity.y = surface.speed;
        excludeContactSurfaces();
        contactState.excluded.insert(surface.id);
        contactState.surfaceId = NoSurface;
    }
    contactState.entrySide = 0;
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
float landingTime(float gap, float speed, float acceleration)
{
    if (acceleration == 0) {
        if (gap == 0 && speed >= 0) {
            return 0;
        }
        return speed > 0 ? -gap / speed : std::numeric_limits<float>::infinity();
    }
    const float root = std::sqrt(std::max(0.0F, speed * speed - 2 * acceleration * gap));
    return speed >= 0 && root + speed > 0 ? -2 * gap / (root + speed) : (root - speed) / acceleration;
}
}

Engine::Vector2 Trajectory::positionAt(float time) const
{
    return origin + velocity * time + acceleration * (time * time / 2);
}

Engine::Vector2 Trajectory::velocityAt(float time) const
{
    return velocity + acceleration * time;
}

CatPhysics::MotionStep CatPhysics::planMotion(float duration, const MotionRequest& request) const
{
    const float difference = request.horizontalTarget - body.velocity.x;
    const float acceleration = request.horizontalAcceleration;
    const Engine::Vector2 accelerationVector{std::abs(difference) < PositionTolerance ? 0 : std::copysign(acceleration, difference),
        request.verticalAcceleration};
    const float targetTime = acceleration == 0 ? std::numeric_limits<float>::infinity() : std::abs(difference) / acceleration;
    if (accelerationVector.x != 0) {
        duration = std::min(duration, targetTime);
        if (body.velocity.x * accelerationVector.x < 0) {
            duration = std::min(duration, -body.velocity.x / accelerationVector.x);
        }
    }
    return MotionStep{Trajectory{body.position, body.velocity, accelerationVector},
        VelocityLimit{request.horizontalTarget, targetTime}, duration};
}

void CatPhysics::limitSupportedMotion(const Surface& standing, bool entering, const Trajectory& trajectory, StepEvent& event) const
{
    if (entering && static_cast<float>(contactState.entrySide) * trajectory.velocity.x <= PositionTolerance && static_cast<float>(contactState.entrySide) * trajectory.acceleration.x < 0) {
        event.time = 0;
        event.kind = StepEventKind::ContactEnd;
    }
    // Мост может исчезнуть раньше достижения горизонтального края.
    const float widestBridge = body.supportShape.segments[1].right - body.supportShape.segments[0].left - 2 * body.supportShape.minimumOverlap + 2 * body.supportShape.bridgeMargin;
    for (const auto* neighborPointer : surfaces) {
        const auto& neighbor = *neighborPointer;
        const float leftWidth = neighbor.position.x < standing.position.x ? neighbor.size.width : standing.size.width;
        const float gap = std::abs(neighbor.position.x - standing.position.x) - leftWidth;
        const float relativeSpeed = neighbor.speed - standing.speed;
        if (neighbor.id == contactState.surfaceId || contactState.excluded.count(neighbor.id) != 0
            || gap <= 0 || gap > widestBridge || relativeSpeed == 0) {
            continue;
        }
        for (const float height : std::initializer_list<float>{-body.supportShape.heightAllowance, body.supportShape.heightAllowance}) {
            const float time = (height - (neighbor.position.y - standing.position.y)) / relativeSpeed;
            if (time > TimeTolerance) {
                event.time = std::min(event.time, time);
            }
        }
    }
    float destination = trajectory.positionAt(event.time).x;
    if (entering) {
        const float boundary = standing.position.x + (contactState.entrySide > 0 ? -body.supportShape.overhang : standing.size.width + body.supportShape.overhang);
        const auto settled = *supportInterval(standing, boundary, SupportQuery{0, 0, false});
        const float entryX = contactState.entrySide > 0 ? settled.left : settled.right;
        if ((contactState.entrySide > 0 && destination >= entryX) || (contactState.entrySide < 0 && destination <= entryX)) {
            event.time = std::clamp(travelTime(trajectory.velocity.x, trajectory.acceleration.x, entryX - trajectory.origin.x), 0.0F, event.time);
            destination = trajectory.positionAt(event.time).x;
        }
    }
    if (const auto range = *supportInterval(standing, trajectory.origin.x, SupportQuery{trajectory.velocity.x, 0, true});
        destination > range.right + PositionTolerance || destination < range.left - PositionTolerance) {
        event.edgeX = destination > trajectory.origin.x ? range.right : range.left;
        event.time = std::clamp(travelTime(trajectory.velocity.x, trajectory.acceleration.x, event.edgeX - trajectory.origin.x), 0.0F, event.time);
        event.kind = StepEventKind::ContactEnd;
    }
}

void CatPhysics::findAirborneEvent(const Trajectory& trajectory, StepEvent& event, std::optional<float> maximumY) const
{
    for (const auto* surfacePointer : surfaces) {
        const auto& surface = *surfacePointer;
        if (contactState.excluded.count(surface.id) != 0) {
            continue;
        }
        const auto land = [&](float time) {
            event.time = std::max(0.0F, time);
            event.kind = StepEventKind::ContactBegin;
            event.surface = &surface;
        };
        // Контакт сверху проверяется первым; равное время сохраняет прежнего кандидата.
        const float gap = trajectory.origin.y - surface.position.y;
        const float topTime = landingTime(gap, trajectory.velocity.y - surface.speed, trajectory.acceleration.y);
        if (gap <= PositionTolerance && topTime >= -TimeTolerance && topTime <= event.time + TimeTolerance
            && supportInterval(surface, trajectory.positionAt(topTime).x, SupportQuery{trajectory.velocity.x + trajectory.acceleration.x * topTime, topTime, true})
            && (!event.surface || topTime < event.time - TimeTolerance)) {
            land(topTime);
        }
        // Боковой въезд допускается только к центру опоры и без подъёма относительно неё.
        if (std::abs(trajectory.velocity.x) <= PositionTolerance) {
            continue;
        }
        const auto approach = nextSupportInterval(surface, trajectory.origin.x, SupportQuery{trajectory.velocity.x, 0, true});
        if (!approach) {
            continue;
        }
        const float entryX = std::clamp(trajectory.origin.x, approach->left, approach->right);
        const float destination = trajectory.positionAt(event.time).x;
        if ((trajectory.velocity.x > 0 && destination < entryX) || (trajectory.velocity.x < 0 && destination > entryX)) {
            continue;
        }
        const float time = std::abs(entryX - trajectory.origin.x) <= PositionTolerance
            ? 0 : travelTime(trajectory.velocity.x, trajectory.acceleration.x, entryX - trajectory.origin.x);
        const Engine::Vector2 speed = trajectory.velocityAt(time);
        if (time < 0 || time > event.time || (event.surface && time >= event.time - TimeTolerance)
            || (surface.position.x + surface.size.width / 2.0F - entryX) * speed.x <= PositionTolerance
            || speed.y - surface.speed < -PositionTolerance
            || !supportInterval(surface, entryX, SupportQuery{speed.x, time, true})) {
            continue;
        }
        const bool bridging = (entryX < surface.position.x - body.supportShape.overhang
            || entryX > surface.position.x + surface.size.width + body.supportShape.overhang)
            && supportInterval(surface, entryX, SupportQuery{0, time, false});
        const float clearance = trajectory.positionAt(time).y - (surface.position.y + surface.speed * time);
        if (clearance >= -PositionTolerance && clearance <= body.supportShape.heightAllowance * (bridging ? 2 : 1) + PositionTolerance) {
            land(time);
        }
    }
    if (maximumY) {
        const float limitTime = landingTime(trajectory.origin.y - *maximumY, trajectory.velocity.y, trajectory.acceleration.y);
        if (limitTime <= event.time && (!event.surface || limitTime < event.time - TimeTolerance)) {
            event.time = limitTime;
            event.kind = StepEventKind::LimitReached;
        }
    }
}

StepResult CatPhysics::advance(float duration, const MotionRequest& request, std::optional<float> maximumY)
{
    const Surface* standing = findSurface(surfaces, contactState.surfaceId);
    const bool entering = standing && !supportInterval(*standing, body.position.x, SupportQuery{0, 0, false});
    if (!entering) {
        contactState.entrySide = 0;
    } else if (contactState.entrySide == 0) {
        contactState.entrySide = body.velocity.x > 0 ? 1 : -1;
    }
    const MotionStep motion = planMotion(duration, request);
    const Trajectory& trajectory = motion.trajectory;
    StepEvent event{motion.duration, StepEventKind::None, nullptr, body.position.x};
    // Тело и поверхности интегрируются до одного и того же ближайшего события.
    if (standing) {
        limitSupportedMotion(*standing, entering, trajectory, event);
    } else {
        findAirborneEvent(trajectory, event, maximumY);
    }
    body.position = trajectory.positionAt(event.time);
    body.velocity += trajectory.acceleration * event.time;
    if (trajectory.acceleration.x != 0 && std::abs(event.time - motion.velocityLimit.time) <= TimeTolerance) {
        body.velocity.x = motion.velocityLimit.value;
    }
    for (auto* surfacePointer : surfaces) {
        auto& surface = *surfacePointer;
        surface.position.y += surface.speed * event.time;
    }
    if (event.kind == StepEventKind::ContactBegin) {
        contactState.surfaceId = event.surface->id;
        contactState.entrySide = 0;
        contactState.excluded.clear();
    }
    if (const auto* surface = findSurface(surfaces, contactState.surfaceId)) {
        body.position.y = surface->position.y;
        body.velocity.y = surface->speed;
        if (!supportInterval(*surface, body.position.x, SupportQuery{body.velocity.x, 0, true})) {
            leaveEdge(*surface, body.position.x);
        } else if (event.kind == StepEventKind::ContactEnd) {
            leaveEdge(*surface, event.edgeX);
        }
    }
    const int eventSurface = event.surface ? event.surface->id
        : (event.kind == StepEventKind::ContactEnd && standing ? standing->id : NoSurface);
    return StepResult{event.time, event.kind, eventSurface};
}

}
