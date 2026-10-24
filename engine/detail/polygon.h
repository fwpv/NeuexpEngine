#pragma once

#include "math2d.h"

#include <array>
#include <cstddef>
#include <vector>

namespace Engine::Detail
{
// Удаляет соседние повторы и коллинеарные промежуточные точки, проверяет простой
// контур и разбивает его на треугольники. На отказе выходные данные не используются.
bool triangulatePolygon(const std::vector<Vector2>& vertices, std::vector<Vector2>& contour,
    std::vector<std::array<std::size_t, 3>>& triangles);
}
