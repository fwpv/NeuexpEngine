#pragma once

#include "math2d.h"

#include <array>
#include <vector>

namespace Engine
{
// Четырёхугольный полигон: четыре последовательные вершины, замыкание не повторяется.
// Форма не обязана быть квадратом или прямоугольником.
using Quad = std::array<Vector2, 4>;

// Простой замкнутый контур без отверстий и самопересечений; вершины конечные.
// Последняя вершина соединяется с первой автоматически. Любой обход;
// соседние повторы, повтор первой вершины в конце и коллинеарные точки допустимы.
using Polygon = std::vector<Vector2>;

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
        return {position.x + size.width / 2, position.y + size.height / 2};
    }

    // Обход от угла с минимальными x/y: +x, затем +y.
    constexpr Quad corners() const
    {
        return {position, Vector2{position.x + size.width, position.y},
            Vector2{position.x + size.width, position.y + size.height},
            Vector2{position.x, position.y + size.height}};
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
// Граница включена. Пустой полигон не содержит точек; вырожденный содержит
// только точки своих рёбер. point должен быть конечным.
bool contains(const Polygon& polygon, Vector2 point);
bool contains(const Quad& polygon, Vector2 point);
// Для пустого полигона возвращает Rect{}; для вырожденного размер может быть нулевым.
Rect bounds(const Polygon& polygon);
Rect bounds(const Quad& polygon);
// Ближайшая точка замкнутого прямоугольника; внутренняя точка возвращается без изменения.
Vector2 closestPoint(const Rect& rectangle, Vector2 point);
// Касание считается пересечением, в том числе для вырожденных фигур.
bool intersects(const Rect& first, const Rect& second);
bool intersects(const Circle& first, const Circle& second);
bool intersects(const Rect& rectangle, const Circle& circle);
bool intersects(const Circle& circle, const Rect& rectangle);
// При отсутствии пересечения result не меняется. При касании размер по одной из осей равен нулю.
bool intersection(const Rect& first, const Rect& second, Rect& result);
}
