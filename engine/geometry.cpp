#include "geometry.h"
#include "detail/segments.h"
#include "math2d.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace Engine
{
bool contains(const Rect& rectangle, Vector2 point)
{
    return point.x >= rectangle.position.x && point.y >= rectangle.position.y
        && point.x <= rectangle.position.x + rectangle.size.width
        && point.y <= rectangle.position.y + rectangle.size.height;
}

bool contains(const Circle& circle, Vector2 point)
{
    return std::hypot(static_cast<double>(point.x) - circle.center.x,
        static_cast<double>(point.y) - circle.center.y) <= circle.radius;
}

namespace
{
bool containsVertices(const Vector2* vertices, std::size_t count, Vector2 point)
{
    if (count == 0) {
        return false;
    }
    bool inside = false;
    Vector2 previous = vertices[count - 1];
    for (std::size_t index = 0; index < count; ++index) {
        const Vector2 current = vertices[index];
        if (Detail::onSegment(previous, current, point)) {
            return true;
        }
        // Полуоткрытый интервал по Y учитывает вершину один раз. Горизонтальные
        // рёбра не пересекают луч; сравнение через turn не требует деления.
        if ((previous.y > point.y) != (current.y > point.y)) {
            const double side = Detail::turn(previous, current, point);
            if ((current.y > previous.y && side > 0) || (current.y < previous.y && side < 0)) {
                inside = !inside;
            }
        }
        previous = current;
    }
    return inside;
}

Rect vertexBounds(const Vector2* vertices, std::size_t count)
{
    if (count == 0) {
        return {};
    }
    Vector2 minimum = vertices[0];
    Vector2 maximum = minimum;
    for (std::size_t index = 1; index < count; ++index) {
        const Vector2 point = vertices[index];
        minimum.x = std::min(minimum.x, point.x);
        minimum.y = std::min(minimum.y, point.y);
        maximum.x = std::max(maximum.x, point.x);
        maximum.y = std::max(maximum.y, point.y);
    }
    return {minimum, Size2{maximum.x - minimum.x, maximum.y - minimum.y}};
}
}

bool contains(const Polygon& polygon, Vector2 point)
{
    return containsVertices(polygon.data(), polygon.size(), point);
}

bool contains(const Quad& polygon, Vector2 point)
{
    return containsVertices(polygon.data(), polygon.size(), point);
}

Rect bounds(const Polygon& polygon)
{
    return vertexBounds(polygon.data(), polygon.size());
}

Rect bounds(const Quad& polygon)
{
    return vertexBounds(polygon.data(), polygon.size());
}

Vector2 closestPoint(const Rect& rectangle, Vector2 point)
{
    return {
        std::clamp(point.x, rectangle.position.x, rectangle.position.x + rectangle.size.width),
        std::clamp(point.y, rectangle.position.y, rectangle.position.y + rectangle.size.height)
    };
}

bool intersects(const Rect& first, const Rect& second)
{
    return first.position.x <= second.position.x + second.size.width
        && second.position.x <= first.position.x + first.size.width
        && first.position.y <= second.position.y + second.size.height
        && second.position.y <= first.position.y + first.size.height;
}

bool intersects(const Circle& first, const Circle& second)
{
    return std::hypot(static_cast<double>(first.center.x) - second.center.x,
        static_cast<double>(first.center.y) - second.center.y)
        <= static_cast<double>(first.radius) + second.radius;
}

bool intersects(const Rect& rectangle, const Circle& circle)
{
    return contains(circle, closestPoint(rectangle, circle.center));
}

bool intersects(const Circle& circle, const Rect& rectangle)
{
    return intersects(rectangle, circle);
}

bool intersection(const Rect& first, const Rect& second, Rect& result)
{
    const float x = std::max(first.position.x, second.position.x);
    const float y = std::max(first.position.y, second.position.y);
    const float right = std::min(first.position.x + first.size.width, second.position.x + second.size.width);
    const float bottom = std::min(first.position.y + first.size.height, second.position.y + second.size.height);
    if (right < x || bottom < y) {
        return false;
    }
    result = {x, y, right - x, bottom - y};
    return true;
}
}
