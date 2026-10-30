#pragma once

#include "geometry.h"

namespace Engine
{
enum class TransformError
{
    None,
    NonInvertible
};

// [m00 m01 m02; m10 m11 m12; 0 0 1], умножение на вектор-столбец (x, y, 1).
// По умолчанию — тождественное преобразование. Коэффициенты должны быть конечными.
struct Affine2
{
    float m00 = 1;
    float m01 = 0;
    float m02 = 0;
    float m10 = 0;
    float m11 = 1;
    float m12 = 0;

    Vector2 transformPoint(Vector2 point) const;
    // Направления и смещения преобразуются без переноса.
    Vector2 transformVector(Vector2 vector) const;
    // Возвращает охватывающий прямоугольник вдоль осей, а не повёрнутую фигуру.
    Rect transformBounds(const Rect& rectangle) const;
    // При вырожденной матрице или непредставимом результате result не меняется.
    // error заменяется при каждом вызове; result может совпадать с *this.
    bool inverse(Affine2& result, TransformError& error) const;

    Affine2& operator*=(const Affine2& other);
};

// first * second сначала применяет second, затем first, сохраняя в том числе неперпендикулярные оси.
Affine2 operator*(const Affine2& first, const Affine2& second);

struct Transform2
{
    Vector2 position;
    // Радианы; положительный поворот направляет +x к +y.
    float rotation = 0;
    Vector2 scale = {1, 1};
    // Локальная точка, которая после преобразования попадает в position.
    Vector2 origin;

    // Порядок: вычесть origin, масштабировать, повернуть, прибавить position.
    Affine2 toAffine() const;
};
}
