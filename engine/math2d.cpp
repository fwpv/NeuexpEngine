#include "math2d.h"

#include <cmath>

namespace Engine
{
float length(Vector2 value)
{
    return std::hypot(value.x, value.y);
}

float distance(Vector2 first, Vector2 second)
{
    return length(second - first);
}

Vector2 normalized(Vector2 value)
{
    const double magnitude = std::hypot(static_cast<double>(value.x), static_cast<double>(value.y));
    if (magnitude == 0) {
        return {};
    }
    return {static_cast<float>(value.x / magnitude), static_cast<float>(value.y / magnitude)};
}

Vector2 rotated(Vector2 value, float radians)
{
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {cosine * value.x - sine * value.y, sine * value.x + cosine * value.y};
}
}
