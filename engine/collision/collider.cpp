#include "collider.h"

#include <cmath>

namespace NeuexpEngine
{
Collider::Collider(const CollisionShape& value)
    : shape(value)
    , enabled(true)
{
}

CollisionShape Collider::worldShape(const Transform2& transform) const
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

bool canCollide(const Collider& first, const Collider& second)
{
    if (!first.enabled || !second.enabled) {
        return false;
    }
    return (first.layer & second.mask) != 0 && (second.layer & first.mask) != 0;
}
}
