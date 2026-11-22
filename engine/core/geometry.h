#pragma once

#include "math2d.h"

#include <array>
#include <vector>

namespace NeuexpEngine
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

    Vector2 position;
    Size2 size;
};

// Радиус неотрицательный. Нулевые размеры фигур допустимы.
struct Circle
{
    constexpr Circle() = default;
    constexpr Circle(Vector2 center, float radius)
        : center(center)
        , radius(radius)
    {
    }

    Vector2 center;
    float radius = 0;
};

// Прямоугольник, повёрнутый вокруг центра; размеры неотрицательные, границы включены.
// rotation в радианах направляет +x к +y, при нулевом повороте форма равна Rect.
struct Box
{
    constexpr Box() = default;
    constexpr Box(Vector2 center, Size2 size, float rotation = 0)
        : center(center)
        , size(size)
        , rotation(rotation)
    {
    }

    // Обход от локального угла с минимальными x/y: +x, затем +y.
    Quad corners() const;

    Vector2 center;
    Size2 size;
    float rotation = 0;
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

constexpr bool operator==(const Box& first, const Box& second)
{
    return first.center == second.center && first.size == second.size && first.rotation == second.rotation;
}

constexpr bool operator!=(const Box& first, const Box& second)
{
    return !(first == second);
}

bool contains(const Rect& rectangle, Vector2 point);
bool contains(const Circle& circle, Vector2 point);
// Граница включена; сравнение идёт в осях прямоугольника.
bool contains(const Box& box, Vector2 point);
// Граница включена. Пустой полигон не содержит точек; вырожденный содержит
// только точки своих рёбер. point должен быть конечным.
bool contains(const Polygon& polygon, Vector2 point);
bool contains(const Quad& polygon, Vector2 point);
// Для пустого полигона возвращает Rect{}; для вырожденного размер может быть нулевым.
Rect bounds(const Polygon& polygon);
Rect bounds(const Quad& polygon);
// Охватывающий прямоугольник вдоль осей; для нулевого поворота совпадает с исходным размером.
Rect bounds(const Box& box);
// Ближайшая точка замкнутого прямоугольника; внутренняя точка возвращается без изменения.
Vector2 closestPoint(const Rect& rectangle, Vector2 point);
Vector2 closestPoint(const Box& box, Vector2 point);
// Касание считается пересечением, в том числе для вырожденных фигур.
bool intersects(const Rect& first, const Rect& second);
bool intersects(const Circle& first, const Circle& second);
bool intersects(const Rect& rectangle, const Circle& circle);
bool intersects(const Circle& circle, const Rect& rectangle);
// При отсутствии пересечения result не меняется. При касании размер по одной из осей равен нулю.
bool intersection(const Rect& first, const Rect& second, Rect& result);
}
