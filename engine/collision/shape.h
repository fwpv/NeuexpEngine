#pragma once

#include "core/geometry.h"

namespace NeuexpEngine
{
// Форма столкновений: круг, прямоугольник вдоль осей или повёрнутый прямоугольник.
// Хранится одна из форм, а вид указывает, какая именно; поля остальных видов не используются.
class CollisionShape
{
public:
    enum class Kind
    {
        Circle,
        Rect,
        Box
    };

    CollisionShape() = default;
    CollisionShape(const Circle& value);
    CollisionShape(const Rect& value);
    CollisionShape(const Box& value);

    Kind kind() const { return shapeKind; }

    // Заполненную форму возвращает только метод её вида: остальные проверяют assert.
    const Circle& circle() const;
    const Rect& rect() const;
    const Box& box() const;

private:
    Kind shapeKind = Kind::Circle;
    Circle circleValue;
    Rect rectValue;
    Box boxValue;
};

// Охватывающий прямоугольник вдоль осей.
Rect bounds(const CollisionShape& shape);
// Граница включена.
bool contains(const CollisionShape& shape, Vector2 point);
}
