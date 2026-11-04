#pragma once

#include "texture.h"
#include "transform.h"
#include "geometry.h"
#include "math2d.h"

#include <optional>

namespace Engine
{
struct Sprite
{
    Sprite() = default;
    // Начальный размер равен размеру текстуры или участка; затем меняется независимо.
    explicit Sprite(const Texture& texture);
    Sprite(const Texture& texture, const Rect& textureRect);

    // Четыре угла после transform, без преобразований DrawOptions и камеры.
    // Не зависит от текстуры, visible и альфа-канала; size должен быть неотрицательным.
    Quad polygon() const;

    Texture texture;
    // Пиксели исходной текстуры, начало сверху слева. nullopt — вся текстура.
    // Участок должен быть конечным, с неотрицательными размерами и внутри текстуры.
    std::optional<Rect> textureRect;
    // Неотрицательный размер в локальных координатах, до применения transform.
    Size2 size;
    // origin задаётся в локальных единицах size, rotation — в радианах.
    Transform2 transform;
    Color color;
    bool visible = true;
    TextureFilter filter = TextureFilter::Linear;
};
}
