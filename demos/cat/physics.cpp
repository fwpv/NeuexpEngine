#include "physics.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Cat
{
namespace
{
constexpr float ContactTolerance = 0.01F;
constexpr float DropDuration = 0.15F;
}

CatPhysics::CatPhysics(Body& body, const std::vector<Surface*>& surfaces)
    : body(body)
    , surfaces(surfaces)
{
}

bool CatPhysics::supports(int surfaceId) const
{
    return surfaceId >= 0 && supportId == surfaceId;
}

bool CatPhysics::hasSupport() const
{
    return supportId >= 0;
}

void CatPhysics::reset(Engine::Vector2 position, Engine::Vector2 velocity)
{
    body.position = position;
    body.velocity = velocity;
    supportId = -1;
    dropTime = 0;
}

void CatPhysics::launch(float upwardImpulse)
{
    supportId = -1;
    body.velocity.y -= upwardImpulse;
}

void CatPhysics::dropThrough(float downwardImpulse)
{
    supportId = -1;
    dropTime = DropDuration;
    body.velocity.y = std::max(downwardImpulse, body.velocity.y + downwardImpulse);
}

bool CatPhysics::canSupport(const Surface& surface) const
{
    const SupportShape& shape = body.supportShape;
    const float x = body.position.x;
    if (x >= surface.position.x - shape.overhang
        && x <= surface.position.x + surface.size.width + shape.overhang) {
        return true;
    }
    for (const auto* neighbor : surfaces) {
        if (neighbor->id == surface.id
            || std::abs(neighbor->position.y - surface.position.y) > shape.heightAllowance) {
            continue;
        }
        const Surface& left = surface.position.x < neighbor->position.x ? surface : *neighbor;
        const Surface& right = surface.position.x < neighbor->position.x ? *neighbor : surface;
        if (left.position.x + left.size.width >= right.position.x) {
            continue;
        }
        const float leftOverlap = std::min(x + shape.segments[0].right, left.position.x + left.size.width)
            - std::max(x + shape.segments[0].left, left.position.x);
        const float rightOverlap = std::min(x + shape.segments[1].right, right.position.x + right.size.width)
            - std::max(x + shape.segments[1].left, right.position.x);
        if (leftOverlap >= shape.minimumOverlap && rightOverlap >= shape.minimumOverlap) {
            return true;
        }
    }
    return false;
}

void CatPhysics::update(float duration, const MotionRequest& request)
{
    if (duration == 0) {
        return;
    }
    const Engine::Vector2 previousPosition = body.position;
    const bool dropping = dropTime > 0;
    dropTime = std::max(0.0F, dropTime - duration);
    std::vector<float> previousHeights;
    previousHeights.reserve(surfaces.size());
    const Surface* standing = nullptr;
    for (auto* surface : surfaces) {
        previousHeights.push_back(surface->position.y);
        if (surface->speed != 0) {
            surface->position.y += surface->speed * duration;
            if (surface->position.y <= surface->top) {
                surface->position.y = surface->top;
                surface->speed = std::abs(surface->speed);
            } else if (surface->position.y >= surface->bottom) {
                surface->position.y = surface->bottom;
                surface->speed = -std::abs(surface->speed);
            }
        }
        if (surface->id == supportId) {
            standing = surface;
        }
    }

    // До целевой скорости ускорение постоянно; оставшееся время движение равномерное.
    const float change = std::clamp(request.horizontalTarget - body.velocity.x,
        -request.horizontalAcceleration * duration, request.horizontalAcceleration * duration);
    const float acceleratingTime = request.horizontalAcceleration > 0 ? std::abs(change) / request.horizontalAcceleration : 0;
    body.position.x += body.velocity.x * duration + change * (duration - acceleratingTime / 2);
    body.velocity.x += change;

    if (standing && !dropping) {
        body.position.y = standing->position.y;
        body.velocity.y = standing->speed;
        if (!canSupport(*standing)) {
            standing = nullptr;
            for (const auto* surface : surfaces) {
                if (std::abs(surface->position.y - body.position.y) <= body.supportShape.heightAllowance
                    && canSupport(*surface)
                    && (!standing || std::abs(surface->position.y - body.position.y)
                        < std::abs(standing->position.y - body.position.y))) {
                    standing = surface;
                }
            }
        }
        if (standing) {
            body.position.y = standing->position.y;
            body.velocity.y = standing->speed;
            supportId = standing->id;
            return;
        }
    }

    supportId = -1;
    body.position.y = previousPosition.y + body.velocity.y * duration + request.verticalAcceleration * duration * duration / 2;
    body.velocity.y += request.verticalAcceleration * duration;
    if (dropping) {
        return;
    }
    // При нескольких пересечениях выбираем первое по доле изменения относительной высоты.
    float firstCrossing = 2;
    const Surface* landing = nullptr;
    for (std::size_t index = 0; index < surfaces.size(); ++index) {
        const Surface& surface = *surfaces[index];
        const float before = previousPosition.y - previousHeights[index];
        const float after = body.position.y - surface.position.y;
        if (before > ContactTolerance || after < 0 || after <= before || !canSupport(surface)) {
            continue;
        }
        const float crossing = std::clamp(-before / (after - before), 0.0F, 1.0F);
        if (crossing < firstCrossing) {
            firstCrossing = crossing;
            landing = &surface;
        }
    }
    if (landing) {
        supportId = landing->id;
        body.position.y = landing->position.y;
        body.velocity.y = landing->speed;
    }
}
}
