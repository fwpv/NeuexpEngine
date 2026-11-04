#pragma once

#include "math2d.h"

#include <algorithm>

namespace Engine::Detail
{
inline double turn(Vector2 a, Vector2 b, Vector2 c)
{
    return (static_cast<double>(b.x) - a.x) * (static_cast<double>(c.y) - a.y)
        - (static_cast<double>(b.y) - a.y) * (static_cast<double>(c.x) - a.x);
}

inline bool onSegment(Vector2 a, Vector2 b, Vector2 point)
{
    return turn(a, b, point) == 0
        && point.x >= std::min(a.x, b.x) && point.x <= std::max(a.x, b.x)
        && point.y >= std::min(a.y, b.y) && point.y <= std::max(a.y, b.y);
}
}
