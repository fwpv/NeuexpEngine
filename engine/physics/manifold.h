#pragma once

#include "collision/shape.h"
#include "core/math2d.h"

#include <array>
#include <cstdint>

namespace NeuexpEngine
{
// Точка контакта: положение в мире, глубина проникновения вдоль normal и
// идентификатор особенности форм (грани/вершины). Идентификатор устойчив между
// шагами, пока контактирует та же пара особенностей: по нему тёплый старт
// переносит импульсы прошлого шага.
struct ContactPoint
{
    Vector2 position;
    float depth = 0;
    std::uint16_t id = 0;
};

// Контакт двух форм для решателя физического мира. normal направлена от первой формы
// ко второй: сдвиг второй формы на normal * depth убирает проникновение. Точек не
// больше двух; для пары граней их две, иначе одна.
struct ContactManifold
{
    Vector2 normal = {1, 0};
    float depth = 0;
    std::array<ContactPoint, 2> points;
    int count = 0;
};

// Касание считается контактом. false — формы разошлись, result не меняется.
// Формы передаются вместе с их положением в мире: круг без поворота, прямоугольник
// вдоль осей или повёрнутый прямоугольник.
bool detectContact(const CollisionShape& first, const CollisionShape& second, ContactManifold& result);
}
