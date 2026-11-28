#pragma once

#include "core/transform.h"
#include "shape.h"

#include <cstdint>

namespace NeuexpEngine
{
// Слой столкновений: бит в маске.
using CollisionLayer = std::uint32_t;
// Маска со всеми слоями.
constexpr CollisionLayer AllCollisionLayers = 0xFFFFFFFFu;

// Форма столкновений объекта и правила участия в парах. Форма задана в локальных
// координатах, в мир её переводит transform объекта. Круг и повёрнутый прямоугольник
// требуют равномерного масштаба: иначе форма в мире становится эллипсом или
// параллелограммом, которые проверки не поддерживают.
struct Collider
{
    // Пустой коллайдер выключен: объект без формы не участвует в столкновениях.
    Collider() = default;
    // Коллайдер с формой сразу включён.
    explicit Collider(const CollisionShape& value);

    // Форма в мировых координатах с учётом преобразования объекта.
    CollisionShape worldShape(const Transform2& transform) const;

    CollisionShape shape;
    CollisionLayer layer = 1U;
    // Слои, с которыми разрешены пары: объект участвует, если его слой входит в маску
    // другого коллайдера, а слой другого — в его маску.
    CollisionLayer mask = AllCollisionLayers;
    // Поверхностные свойства: у пары трение сочетается средним геометрическим,
    // упругость — большей из двух. Материал принадлежит поверхности коллайдера,
    // поэтому статическая геометрия тоже может иметь своё трение и упругость.
    float friction = 0.5F;
    float restitution = 0;
    bool enabled = false;
    // Триггер сообщает о контакте, но не участвует в разрешении движения объектов.
    bool trigger = false;
};

// Пара участвует в проверке: оба коллайдера включены и слои входят в маски друг друга.
bool canCollide(const Collider& first, const Collider& second);
// Совместимость слоёв пары: слой каждого входит в маску другого.
bool layersMatch(CollisionLayer firstLayer, CollisionLayer firstMask, CollisionLayer secondLayer,
    CollisionLayer secondMask);
}
