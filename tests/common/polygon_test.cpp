#include "core/geometry.h"
#include "core/transform.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}
}

int main()
{
    try {
        using namespace NeuexpEngine;
        const Polygon empty;
        require(bounds(empty) == Rect{} && !contains(empty, Vector2{}), "Empty polygon failed.");
        const Polygon concave = {Vector2{-2, -1}, Vector2{4, -1}, Vector2{4, 1},
            Vector2{0, 1}, Vector2{0, 5}, Vector2{-2, 5}};
        require(bounds(concave) == Rect{-2, -1, 6, 6}, "Concave polygon bounds failed.");
        require(contains(concave, Vector2{3, 0}) && contains(concave, Vector2{-1, 4}), "Interior points excluded.");
        require(!contains(concave, Vector2{3, 4}) && !contains(concave, Vector2{-3, 0}), "Exterior points included.");
        for (Vector2 vertex : concave) {
            require(contains(concave, vertex), "Polygon vertex excluded.");
        }
        require(contains(concave, Vector2{2, -1}) && contains(concave, Vector2{0, 3}), "Polygon edge excluded.");

        Polygon redundant = {Vector2{-2, -1}, Vector2{1, -1}, Vector2{4, -1}, Vector2{4, 1},
            Vector2{0, 1}, Vector2{0, 5}, Vector2{-2, 5}};
        redundant.push_back(redundant.back());
        redundant.push_back(redundant.front());
        std::reverse(redundant.begin(), redundant.end());
        for (int y = -2; y <= 6; ++y) {
            for (int x = -3; x <= 5; ++x) {
                const Vector2 point = {static_cast<float>(x), static_cast<float>(y)};
                require(contains(redundant, point) == contains(concave, point), "Containment depends on contour representation.");
            }
        }

        const Quad diamond = {Vector2{0, -2}, Vector2{2, 0}, Vector2{0, 2}, Vector2{-2, 0}};
        require(contains(diamond, Vector2{}) && contains(diamond, Vector2{1, 1})
            && !contains(diamond, Vector2{1.1F, 1}), "Slanted edge or vertex ray failed.");
        const Rect rectangle = {-3, 2, 8, 5};
        require(rectangle.corners() == Quad{Vector2{-3, 2}, Vector2{5, 2}, Vector2{5, 7}, Vector2{-3, 7}}
            && bounds(rectangle.corners()) == rectangle, "Rectangle corner order or bounds failed.");
        const Quad concaveQuad = {Vector2{0, 0}, Vector2{4, 0}, Vector2{1, 1}, Vector2{0, 4}};
        const Polygon equivalent(concaveQuad.begin(), concaveQuad.end());
        require(contains(concaveQuad, Vector2{0.5F, 0.5F}) && contains(concaveQuad, Vector2{0, 2})
            && !contains(concaveQuad, Vector2{2, 2}), "Concave quad containment failed.");
        for (int y = -1; y <= 5; ++y) {
            for (int x = -1; x <= 5; ++x) {
                const Vector2 sample = {static_cast<float>(x), static_cast<float>(y)};
                require(contains(concaveQuad, sample) == contains(equivalent, sample), "Quad and polygon containment differ.");
            }
        }

        const Polygon point = {Vector2{3, 4}};
        const Polygon line = {Vector2{1, 2}, Vector2{3, 4}, Vector2{5, 6}};
        require(bounds(point) == Rect{3, 4, 0, 0} && contains(point, Vector2{3, 4})
            && !contains(point, Vector2{3, 5}), "Point polygon failed.");
        require(contains(line, Vector2{2, 3}) && !contains(line, Vector2{2, 4})
            && !contains(line, Vector2{0, 1}), "Degenerate polygon failed.");

        // Точные двоичные коэффициенты позволяют проверить границу после сдвига, отражения и сдвига осей.
        const Affine2 affine = {-2, 0.5F, 10, 0, 3, -4};
        const Quad mappedQuad = transformed(concaveQuad, affine);
        const Polygon mappedEquivalent = transformed(equivalent, affine);
        require(std::equal(mappedQuad.begin(), mappedQuad.end(), mappedEquivalent.begin())
            && bounds(mappedQuad) == bounds(mappedEquivalent), "Quad and polygon transforms or bounds differ.");
        const Polygon mapped = transformed(concave, affine);
        require(mapped.size() == concave.size(), "Transform changed vertex count.");
        for (std::size_t index = 0; index < concave.size(); ++index) {
            require(mapped[index] == affine.transformPoint(concave[index]), "Transform changed vertex order.");
        }
        require(contains(mapped, affine.transformPoint(Vector2{-1, 4}))
            && !contains(mapped, affine.transformPoint(Vector2{3, 4})), "Transformed containment failed.");
        require(bounds(mapped) == affine.transformBounds(bounds(concave)), "Transformed bounds failed.");
        require(concave.front() == Vector2{-2, -1}, "Transform modified input.");
        require(transformed(empty, affine).empty(), "Transform added vertices to empty polygon.");
        const Quad collapsed = transformed(diamond, Transform2{Vector2{10, 20}, 0, Vector2{0, 0}, Vector2{}}.toAffine());
        require(bounds(collapsed) == Rect{10, 20, 0, 0} && contains(collapsed, Vector2{10, 20})
            && !contains(collapsed, Vector2{10, 21}), "Collapsed polygon failed.");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
