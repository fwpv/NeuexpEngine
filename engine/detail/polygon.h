#pragma once

#include "math2d.h"

#include <array>
#include <cstddef>
#include <vector>

namespace NeuexpEngine::Detail
{
// Удаляет соседние повторы и коллинеарные промежуточные точки, проверяет простой
// контур и разбивает его на треугольники. На отказе выходные данные не используются.
bool triangulatePolygon(const Vector2* vertices, std::size_t count, std::vector<Vector2>& contour,
    std::vector<std::array<std::size_t, 3>>& triangles);
}
