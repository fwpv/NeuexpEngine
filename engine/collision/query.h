#pragma once

#include "shape.h"

namespace NeuexpEngine
{
// Результат пересечения: normal — единичное направление от первой формы ко второй,
// сдвиг второй на normal * depth убирает проникновение; depth >= 0, при касании 0.
struct Manifold
{
    Vector2 normal = {1, 0};
    float depth = 0;
};

// Касание границ — пересечение; при отказе result не меняется.
bool collide(const CollisionShape& first, const CollisionShape& second, Manifold& result);
bool overlaps(const CollisionShape& first, const CollisionShape& second);

// Расстояние между границами форм; ноль для пересекающихся и касающихся форм.
float distance(const CollisionShape& first, const CollisionShape& second);

// Попадание отрезка: point — первое касание, normal — наружная нормаль против движения,
// fraction — доля длины от 0 до 1. Начало внутри формы даёт попадание в начале с
// нормалью ближайшей грани; совпадающие концы задают точку.
struct SegmentHit
{
    Vector2 point;
    Vector2 normal = {1, 0};
    float fraction = 0;
};

// false — попадания нет, result не меняется. Длинный отрезок задаёт луч.
bool castSegment(const CollisionShape& shape, Vector2 begin, Vector2 end, SegmentHit& result);
}
