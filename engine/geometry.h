#pragma once

#include "math2d.h"

namespace Engine
{
// Прямоугольник вдоль координатных осей. Размеры неотрицательные, границы включены.
// position — угол с минимальными x/y, независимо от направления экранной оси y.
struct Rect
{
    Vector2 position;
    Size2 size;

    constexpr Rect() = default;
    constexpr Rect(Vector2 position, Size2 size)
        : position(position)
        , size(size)
    {
    }

    constexpr Rect(float x, float y, float width, float height)
        : position(x, y)
        , size(width, height)
    {
    }

    constexpr Vector2 center() const
    {
        return { position.x + size.width / 2, position.y + size.height / 2 };
    }
};

// Радиус неотрицательный. Нулевые размеры фигур допустимы.
struct Circle
{
    Vector2 center;
    float radius = 0;

    constexpr Circle() = default;
    constexpr Circle(Vector2 center, float radius)
        : center(center)
        , radius(radius)
    {
    }
};

constexpr bool operator==(const Rect& first, const Rect& second)
{
    return first.position == second.position && first.size == second.size;
}

constexpr bool operator!=(const Rect& first, const Rect& second)
{
    return !(first == second);
}

constexpr bool operator==(const Circle& first, const Circle& second)
{
    return first.center == second.center && first.radius == second.radius;
}

constexpr bool operator!=(const Circle& first, const Circle& second)
{
    return !(first == second);
}

bool contains(const Rect& rectangle, Vector2 point);
bool contains(const Circle& circle, Vector2 point);
// Касание считается пересечением, в том числе для вырожденных фигур.
bool intersects(const Rect& first, const Rect& second);
bool intersects(const Circle& first, const Circle& second);
bool intersects(const Rect& rectangle, const Circle& circle);
bool intersects(const Circle& circle, const Rect& rectangle);
// При отсутствии пересечения result не меняется. При касании размер по одной из осей равен нулю.
bool intersection(const Rect& first, const Rect& second, Rect& result);
}
