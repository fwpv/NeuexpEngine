#include "transform.h"

#include <array>
#include <cmath>
#include <limits>

namespace Engine
{
Vector2 Affine2::transformPoint(Vector2 point) const
{
    return {m00 * point.x + m01 * point.y + m02, m10 * point.x + m11 * point.y + m12};
}

Vector2 Affine2::transformVector(Vector2 vector) const
{
    return {m00 * vector.x + m01 * vector.y, m10 * vector.x + m11 * vector.y};
}

Rect Affine2::transformBounds(const Rect& rectangle) const
{
    return bounds(transformed(rectangle.corners(), *this));
}

bool Affine2::inverse(Affine2& result, TransformError& error) const
{
    const double determinant = static_cast<double>(m00) * m11 - static_cast<double>(m01) * m10;
    if (determinant == 0) {
        error = TransformError::NonInvertible;
        return false;
    }

    const std::array<double, 6> coefficients = {
        m11 / determinant,
        -m01 / determinant,
        (static_cast<double>(m01) * m12 - static_cast<double>(m11) * m02) / determinant,
        -m10 / determinant,
        m00 / determinant,
        (static_cast<double>(m10) * m02 - static_cast<double>(m00) * m12) / determinant
    };
    for (double coefficient : coefficients) {
        if (!std::isfinite(coefficient) || std::abs(coefficient) > std::numeric_limits<float>::max()) {
            error = TransformError::NonInvertible;
            return false;
        }
    }
    result = {
        static_cast<float>(coefficients[0]), static_cast<float>(coefficients[1]), static_cast<float>(coefficients[2]),
        static_cast<float>(coefficients[3]), static_cast<float>(coefficients[4]), static_cast<float>(coefficients[5])
    };
    error = TransformError::None;
    return true;
}

Affine2& Affine2::operator*=(const Affine2& other)
{
    *this = *this * other;
    return *this;
}

Affine2 operator*(const Affine2& first, const Affine2& second)
{
    return {
        first.m00 * second.m00 + first.m01 * second.m10,
        first.m00 * second.m01 + first.m01 * second.m11,
        first.m00 * second.m02 + first.m01 * second.m12 + first.m02,
        first.m10 * second.m00 + first.m11 * second.m10,
        first.m10 * second.m01 + first.m11 * second.m11,
        first.m10 * second.m02 + first.m11 * second.m12 + first.m12
    };
}

namespace
{
template<typename Vertices>
void transformVertices(Vertices& vertices, const Affine2& transform)
{
    for (Vector2& point : vertices) {
        point = transform.transformPoint(point);
    }
}
}

Polygon transformed(const Polygon& polygon, const Affine2& transform)
{
    Polygon result = polygon;
    transformVertices(result, transform);
    return result;
}

Quad transformed(const Quad& polygon, const Affine2& transform)
{
    Quad result = polygon;
    transformVertices(result, transform);
    return result;
}

Affine2 Transform2::toAffine() const
{
    const float cosine = std::cos(rotation);
    const float sine = std::sin(rotation);
    Affine2 result;
    result.m00 = cosine * scale.x;
    result.m01 = -sine * scale.y;
    result.m10 = sine * scale.x;
    result.m11 = cosine * scale.y;
    result.m02 = position.x - result.m00 * origin.x - result.m01 * origin.y;
    result.m12 = position.y - result.m10 * origin.x - result.m11 * origin.y;
    return result;
}
}
