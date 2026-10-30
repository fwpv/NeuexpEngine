#include "geometry.h"

#include <algorithm>
#include <cmath>

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
