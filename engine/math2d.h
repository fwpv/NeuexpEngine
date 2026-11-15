#pragma once

#include <cassert>

namespace NeuexpEngine
{
inline constexpr float Pi = 3.14159265358979323846F;

constexpr float degreesToRadians(float degrees)
{
    return degrees * (Pi / 180.0F);
}

constexpr float radiansToDegrees(float radians)
{
    return radians * (180.0F / Pi);
}

// Общий тип координат, смещений, скоростей и направлений. Единицы задаёт вызывающий код.
// Конечные аргументы и представимые результаты арифметики — ответственность вызывающего кода.
// Делители должны быть ненулевыми; нарушение проверяется assert в отладочной сборке.
struct Vector2
{
    constexpr Vector2() = default;
    constexpr Vector2(float x, float y)
        : x(x)
        , y(y)
    {
    }

    constexpr Vector2& operator+=(Vector2 other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr Vector2& operator-=(Vector2 other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    constexpr Vector2& operator*=(float scalar)
    {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    constexpr Vector2& operator/=(float scalar)
    {
        assert(scalar != 0);
        x /= scalar;
        y /= scalar;
        return *this;
    }

    float x = 0;
    float y = 0;
};

constexpr Vector2 operator+(Vector2 value)
{
    return value;
}

constexpr Vector2 operator-(Vector2 value)
{
    return {-value.x, -value.y};
}

constexpr Vector2 operator+(Vector2 first, Vector2 second)
{
    return first += second;
}

constexpr Vector2 operator-(Vector2 first, Vector2 second)
{
    return first -= second;
}

constexpr Vector2 operator*(Vector2 value, float scalar)
{
    return value *= scalar;
}

constexpr Vector2 operator*(float scalar, Vector2 value)
{
    return value *= scalar;
}

constexpr Vector2 operator/(Vector2 value, float scalar)
{
    return value /= scalar;
}

// Равенство точное; допуск при сравнении результатов вычислений выбирает вызывающий код.
constexpr bool operator==(Vector2 first, Vector2 second)
{
    return first.x == second.x && first.y == second.y;
}

constexpr bool operator!=(Vector2 first, Vector2 second)
{
    return !(first == second);
}

constexpr float dot(Vector2 first, Vector2 second)
{
    return first.x * second.x + first.y * second.y;
}

// Знаковая z-компонента векторного произведения.
constexpr float cross(Vector2 first, Vector2 second)
{
    return first.x * second.y - first.y * second.x;
}

constexpr float lengthSquared(Vector2 value)
{
    return dot(value, value);
}

float length(Vector2 value);
float distance(Vector2 first, Vector2 second);
// Нулевой вектор остаётся нулевым.
Vector2 normalized(Vector2 value);
// Угол в радианах. Положительный поворот направляет ось +x к оси +y.
Vector2 rotated(Vector2 value, float radians);

constexpr Vector2 lerp(Vector2 first, Vector2 second, float amount)
{
    return first + (second - first) * amount;
}

// Конечность размеров и представимость результатов — предусловия арифметики.
struct Size2
{
    constexpr Size2() = default;
    constexpr Size2(float width, float height)
        : width(width)
        , height(height)
    {
    }

    constexpr Size2& operator*=(float scalar)
    {
        width *= scalar;
        height *= scalar;
        return *this;
    }

    constexpr Size2& operator/=(float scalar)
    {
        assert(scalar != 0);
        width /= scalar;
        height /= scalar;
        return *this;
    }

    float width = 0;
    float height = 0;
};

constexpr Size2 operator*(Size2 value, float scalar)
{
    return value *= scalar;
}

constexpr Size2 operator*(float scalar, Size2 value)
{
    return value *= scalar;
}

constexpr Size2 operator/(Size2 value, float scalar)
{
    return value /= scalar;
}

constexpr bool operator==(Size2 first, Size2 second)
{
    return first.width == second.width && first.height == second.height;
}

constexpr bool operator!=(Size2 first, Size2 second)
{
    return !(first == second);
}

// RGBA-компоненты от 0 до 1; автоматическое ограничение значений не выполняется.
struct Color
{
    constexpr Color() = default;
    constexpr Color(float red, float green, float blue, float alpha = 1)
        : red(red)
        , green(green)
        , blue(blue)
        , alpha(alpha)
    {
    }

    float red = 1;
    float green = 1;
    float blue = 1;
    float alpha = 1;
};

constexpr bool operator==(Color first, Color second)
{
    return first.red == second.red && first.green == second.green
        && first.blue == second.blue && first.alpha == second.alpha;
}

constexpr bool operator!=(Color first, Color second)
{
    return !(first == second);
}
}
