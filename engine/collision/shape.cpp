#include "shape.h"

#include <cassert>

namespace NeuexpEngine
{
CollisionShape::CollisionShape(const Circle& value)
    : shapeKind(Kind::Circle)
    , circleValue(value)
{
}

CollisionShape::CollisionShape(const Rect& value)
    : shapeKind(Kind::Rect)
    , rectValue(value)
{
}

CollisionShape::CollisionShape(const Box& value)
    : shapeKind(Kind::Box)
    , boxValue(value)
{
}

const Circle& CollisionShape::circle() const
{
    assert(shapeKind == Kind::Circle);
    return circleValue;
}

const Rect& CollisionShape::rect() const
{
    assert(shapeKind == Kind::Rect);
    return rectValue;
}

const Box& CollisionShape::box() const
{
    assert(shapeKind == Kind::Box);
    return boxValue;
}

Rect bounds(const CollisionShape& shape)
{
    if (shape.kind() == CollisionShape::Kind::Circle) {
        const Circle& circle = shape.circle();
        return {circle.center - Vector2{circle.radius, circle.radius},
            Size2{circle.radius * 2, circle.radius * 2}};
    }
    if (shape.kind() == CollisionShape::Kind::Rect) {
        return shape.rect();
    }
    return bounds(shape.box());
}

bool contains(const CollisionShape& shape, Vector2 point)
{
    if (shape.kind() == CollisionShape::Kind::Circle) {
        return contains(shape.circle(), point);
    }
    if (shape.kind() == CollisionShape::Kind::Rect) {
        return contains(shape.rect(), point);
    }
    return contains(shape.box(), point);
}
}
