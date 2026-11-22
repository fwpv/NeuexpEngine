#include "collision/collider.h"
#include "collision/shape.h"
#include "core/math2d.h"
#include "core/transform.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
constexpr float Tolerance = 1e-4F;

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool closeTo(float first, float second, float tolerance = Tolerance)
{
    return std::fabs(first - second) <= tolerance;
}

bool closeTo(NeuexpEngine::Vector2 first, NeuexpEngine::Vector2 second, float tolerance = Tolerance)
{
    return closeTo(first.x, second.x, tolerance) && closeTo(first.y, second.y, tolerance);
}

void requireCircle(const NeuexpEngine::CollisionShape& shape, NeuexpEngine::Vector2 center, float radius,
    const char* message)
{
    require(shape.kind() == NeuexpEngine::CollisionShape::Kind::Circle, message);
    require(closeTo(shape.circle().center, center) && closeTo(shape.circle().radius, radius), message);
}

void requireRect(const NeuexpEngine::CollisionShape& shape, NeuexpEngine::Rect rectangle, const char* message)
{
    require(shape.kind() == NeuexpEngine::CollisionShape::Kind::Rect, message);
    require(closeTo(shape.rect().position, rectangle.position)
        && closeTo(shape.rect().size.width, rectangle.size.width)
        && closeTo(shape.rect().size.height, rectangle.size.height), message);
}

void requireBox(const NeuexpEngine::CollisionShape& shape, NeuexpEngine::Vector2 center,
    NeuexpEngine::Size2 size, float rotation, const char* message)
{
    require(shape.kind() == NeuexpEngine::CollisionShape::Kind::Box, message);
    require(closeTo(shape.box().center, center) && closeTo(shape.box().size.width, size.width)
        && closeTo(shape.box().size.height, size.height) && closeTo(shape.box().rotation, rotation), message);
}
}

int main()
{
    try {
        using namespace NeuexpEngine;

        const Collider empty;
        require(!empty.enabled && empty.layer == 1U && empty.mask == AllCollisionLayers && !empty.trigger,
            "Empty collider must be disabled with default layers.");
        require(!canCollide(empty, empty), "Disabled colliders must not collide.");

        const Collider plain = Collider{Circle{ { 0, 0 }, 2 }};
        require(plain.enabled && plain.shape.kind() == CollisionShape::Kind::Circle,
            "Collider with a shape must be enabled.");
        require(canCollide(plain, plain), "Enabled colliders on one layer must collide.");

        Collider first = Collider{Circle{ { 0, 0 }, 2 }};
        Collider second = Collider{Circle{ { 0, 0 }, 2 }};
        first.mask = 1U;
        second.layer = 2U;
        require(!canCollide(first, second) && !canCollide(second, first),
            "Layer outside the mask must exclude the pair.");
        first.mask = 2U;
        second.mask = 1U;
        require(canCollide(first, second), "Layers inside both masks must collide.");
        second.enabled = false;
        require(!canCollide(first, second), "Disabled collider must exclude the pair.");

        Transform2 placement;
        placement.position = Vector2{10, 5};
        const Collider circle = Collider{Circle{ { 2, 0 }, 1 }};
        requireCircle(circle.worldShape(placement), { 12, 5 }, 1, "Moved circle shape failed.");
        placement.scale = Vector2{3, 3};
        requireCircle(circle.worldShape(placement), { 16, 5 }, 3, "Scaled circle shape failed.");
        placement.scale = Vector2{1, 1};
        placement.rotation = degreesToRadians(90);
        requireCircle(circle.worldShape(placement), { 10, 7 }, 1, "Rotated circle shape failed.");

        const Collider rectangle = Collider{Rect{ 0, 0, 10, 4 }};
        requireRect(rectangle.worldShape(Transform2{}), Rect{ 0, 0, 10, 4 }, "Unchanged rectangle shape failed.");

        Transform2 flat;
        flat.position = Vector2{5, 2};
        requireRect(rectangle.worldShape(flat), Rect{ 5, 2, 10, 4 }, "Moved rectangle shape failed.");
        flat.rotation = degreesToRadians(90);
        requireBox(rectangle.worldShape(flat), { 3, 7 }, { 10, 4 }, degreesToRadians(90),
            "Rotated rectangle shape failed.");
        flat.scale = Vector2{2, 2};
        requireBox(rectangle.worldShape(flat), { 1, 12 }, { 20, 8 }, degreesToRadians(90),
            "Scaled rotated rectangle shape failed.");

        Transform2 anchor;
        anchor.position = Vector2{5, 2};
        anchor.origin = Vector2{5, 2};
        requireRect(rectangle.worldShape(anchor), Rect{ 0, 0, 10, 4 },
            "Origin must move the shape around the anchor point.");

        Transform2 turnedPlacement;
        turnedPlacement.rotation = degreesToRadians(30);
        const Collider box = Collider{Box{ { 1, 2 }, { 4, 2 }, degreesToRadians(15) }};
        requireBox(box.worldShape(turnedPlacement), rotated(Vector2{ 1, 2 }, degreesToRadians(30)),
            Size2{ 4, 2 }, degreesToRadians(45), "Rotated box shape failed.");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
