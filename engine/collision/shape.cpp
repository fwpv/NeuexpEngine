#include "shape.h"

#include <cassert>
#include <cmath>

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

Box boxOf(const CollisionShape& shape)
{
    if (shape.kind() == CollisionShape::Kind::Box) {
        return shape.box();
    }
    const Rect rectangle = shape.rect();
    return {rectangle.center(), rectangle.size, 0};
}

BoxFaceHit nearestBoxFace(Vector2 localPoint, Size2 halfSize)
{
    // Расчёт в double: у больших координат расстояние до грани теряет точность во float.
    BoxFaceHit result{Vector2{-1, 0}, static_cast<double>(localPoint.x) + halfSize.width, 0};
    const double right = static_cast<double>(halfSize.width) - localPoint.x;
    if (right < result.distance) {
        result = {Vector2{1, 0}, right, 1};
    }
    const double top = static_cast<double>(localPoint.y) + halfSize.height;
    if (top < result.distance) {
        result = {Vector2{0, -1}, top, 2};
    }
    const double bottom = static_cast<double>(halfSize.height) - localPoint.y;
    if (bottom < result.distance) {
        result = {Vector2{0, 1}, bottom, 3};
    }
    return result;
}

CollisionShape transformedShape(const CollisionShape& shape, const Transform2& transform)
{
    const Affine2 affine = transform.toAffine();
    // Модуль масштаба: отражение по оси не меняет занятую форму.
    const float scaleX = std::fabs(transform.scale.x);
    const float scaleY = std::fabs(transform.scale.y);
    if (shape.kind() == CollisionShape::Kind::Circle) {
        // Радиус круга берётся по оси x: поддерживается только равномерный масштаб.
        const Circle& circle = shape.circle();
        return Circle{affine.transformPoint(circle.center), circle.radius * scaleX};
    }
    if (shape.kind() == CollisionShape::Kind::Rect) {
        const Rect& rectangle = shape.rect();
        const Size2 size{rectangle.size.width * scaleX, rectangle.size.height * scaleY};
        const Vector2 center = affine.transformPoint(rectangle.center());
        // Без поворота форма остаётся прямоугольником вдоль осей.
        if (transform.rotation == 0) {
            return Rect{center - Vector2{size.width / 2, size.height / 2}, size};
        }
        return Box{center, size, transform.rotation};
    }
    const Box& box = shape.box();
    return Box{affine.transformPoint(box.center),
        Size2{box.size.width * scaleX, box.size.height * scaleY},
        transform.rotation + box.rotation};
}
}
