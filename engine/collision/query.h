#pragma once

#include "shape.h"

namespace NeuexpEngine
{
// Результат распознавания пересечения. normal — единичное направление от первой формы
// ко второй: сдвиг второй формы на normal * depth устраняет проникновение.
// depth неотрицателен и равен нулю, когда формы только касаются границами.
struct Manifold
{
    Vector2 normal = {1, 0};
    float depth = 0;
};

// Касание границ считается пересечением. При отсутствии пересечения result не меняется.
bool collide(const CollisionShape& first, const CollisionShape& second, Manifold& result);
bool overlaps(const CollisionShape& first, const CollisionShape& second);

// Расстояние между границами форм; ноль для пересекающихся и касающихся форм.
float distance(const CollisionShape& first, const CollisionShape& second);

// Попадание отрезка в форму. point — точка первого касания, normal — наружная нормаль
// формы в этой точке (против направления движения), fraction — доля длины отрезка от 0 до 1.
// Отрезок, начинающийся внутри формы или на её границе, попадает в собственное начало
// с нормалью ближайшей грани. Совпадающие конец и начало задают точку.
struct SegmentHit
{
    Vector2 point;
    Vector2 normal = {1, 0};
    float fraction = 0;
};

// false — попадания нет, result не меняется. Длинный отрезок задаёт луч.
bool castSegment(const CollisionShape& shape, Vector2 begin, Vector2 end, SegmentHit& result);
}
