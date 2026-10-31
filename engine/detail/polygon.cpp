#include "polygon.h"
#include "segments.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace Engine::Detail
{
namespace
{
bool segmentsIntersect(Vector2 a, Vector2 b, Vector2 c, Vector2 d)
{
    const double abC = turn(a, b, c);
    const double abD = turn(a, b, d);
    const double cdA = turn(c, d, a);
    const double cdB = turn(c, d, b);
    if (((abC > 0 && abD < 0) || (abC < 0 && abD > 0))
        && ((cdA > 0 && cdB < 0) || (cdA < 0 && cdB > 0))) {
        return true;
    }
    return onSegment(a, b, c) || onSegment(a, b, d) || onSegment(c, d, a) || onSegment(c, d, b);
}
}

bool triangulatePolygon(const Vector2* vertices, std::size_t vertexCount, std::vector<Vector2>& contour,
    std::vector<std::array<std::size_t, 3>>& triangles)
{
    contour.clear();
    triangles.clear();
    for (std::size_t index = 0; index < vertexCount; ++index) {
        const Vector2 point = vertices[index];
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            return false;
        }
        if (contour.empty() || point != contour.back()) {
            contour.push_back(point);
        }
    }
    if (contour.size() > 1 && contour.front() == contour.back()) {
        contour.pop_back();
    }
    bool removed = true;
    while (removed && contour.size() >= 3) {
        removed = false;
        for (std::size_t index = 0; index < contour.size(); ++index) {
            const Vector2 previous = contour[(index + contour.size() - 1) % contour.size()];
            const Vector2 next = contour[(index + 1) % contour.size()];
            if (onSegment(previous, next, contour[index])) {
                contour.erase(contour.begin() + static_cast<std::ptrdiff_t>(index));
                removed = true;
                break;
            }
        }
    }
    const std::size_t count = contour.size();
    if (count < 3) {
        return false;
    }
    double area = 0;
    for (std::size_t index = 0; index < count; ++index) {
        const std::size_t next = (index + 1) % count;
        // Коллинеарный разворот означает наложение соседних рёбер.
        if (turn(contour[index], contour[next], contour[(index + 2) % count]) == 0) {
            return false;
        }
        area += turn(contour[0], contour[index], contour[next]);
        for (std::size_t other = index + 1; other < count; ++other) {
            const std::size_t otherNext = (other + 1) % count;
            if (next != other && otherNext != index
                && segmentsIntersect(contour[index], contour[next], contour[other], contour[otherNext])) {
                return false;
            }
        }
    }
    if (area == 0) {
        return false;
    }
    const double direction = area > 0 ? 1 : -1;
    std::vector<std::size_t> remaining(count);
    std::iota(remaining.begin(), remaining.end(), std::size_t{0});
    while (remaining.size() > 3) {
        bool found = false;
        for (std::size_t index = 0; index < remaining.size(); ++index) {
            const std::size_t a = remaining[(index + remaining.size() - 1) % remaining.size()];
            const std::size_t b = remaining[index];
            const std::size_t c = remaining[(index + 1) % remaining.size()];
            if (direction * turn(contour[a], contour[b], contour[c]) <= 0) {
                continue;
            }
            bool occupied = false;
            for (std::size_t point : remaining) {
                if (point != a && point != b && point != c
                    && direction * turn(contour[a], contour[b], contour[point]) >= 0
                    && direction * turn(contour[b], contour[c], contour[point]) >= 0
                    && direction * turn(contour[c], contour[a], contour[point]) >= 0) {
                    occupied = true;
                    break;
                }
            }
            if (!occupied) {
                triangles.push_back(std::array<std::size_t, 3>{a, b, c});
                remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(index));
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    triangles.push_back(std::array<std::size_t, 3>{remaining[0], remaining[1], remaining[2]});
    return true;
}
}
