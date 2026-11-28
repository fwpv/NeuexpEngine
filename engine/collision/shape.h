#pragma once

#include "core/geometry.h"
#include "core/transform.h"

#include <cstdint>

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

// Форма как прямоугольник: прямоугольник вдоль осей получает нулевой поворот.
Box boxOf(const CollisionShape& shape);
// Ближайшая грань прямоугольника для точки в его осях: наружная нормаль, расстояние
// до грани и номер (0 — левая, 1 — правая, 2 — верхняя, 3 — нижняя).
struct BoxFaceHit
{
    Vector2 normal = {-1, 0};
    double distance = 0;
    std::uint16_t index = 0;
};

BoxFaceHit nearestBoxFace(Vector2 localPoint, Size2 halfSize);

// Форма в мировых координатах: сначала origin, затем масштаб, поворот и перенос.
// Круг и повёрнутый прямоугольник требуют равномерного масштаба: иначе форма
// в мире становится эллипсом или параллелограммом, которые проверки не поддерживают.
CollisionShape transformedShape(const CollisionShape& shape, const Transform2& transform);
}
