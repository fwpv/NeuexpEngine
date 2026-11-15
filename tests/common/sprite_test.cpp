#include "sprite.h"

#include <cmath>
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
        Sprite sprite;
        sprite.size = {20, 10};
        require(sprite.polygon() == Quad{Vector2{0, 0}, Vector2{20, 0},
            Vector2{20, 10}, Vector2{0, 10}}, "Wrong local sprite polygon.");
        sprite.transform = {Vector2{100, 100}, Pi / 2, Vector2{2, 3}, Vector2{10, 5}};
        const Quad polygon = sprite.polygon();
        const Rect rectangle = bounds(polygon);
        require(std::abs(rectangle.position.x - 85) < 0.001F && std::abs(rectangle.position.y - 80) < 0.001F
            && std::abs(rectangle.size.width - 30) < 0.001F && std::abs(rectangle.size.height - 40) < 0.001F,
            "Sprite polygon ignores transform or origin.");
        require(contains(polygon, Vector2{100, 100}) && !contains(polygon, Vector2{80, 100}), "Sprite polygon containment failed.");
        sprite.visible = false;
        sprite.color.alpha = 0;
        sprite.textureRect = Rect{5, 10, 30, 40};
        require(sprite.polygon() == polygon, "Sprite polygon depends on appearance.");
        sprite.transform = {Vector2{30, 40}, 0, Vector2{-2, 1}, Vector2{}};
        require(bounds(sprite.polygon()) == Rect{-10, 40, 40, 10}, "Reflected sprite polygon failed.");
        sprite.transform.scale = {0, 1};
        require(bounds(sprite.polygon()) == Rect{30, 40, 0, 10}
            && contains(sprite.polygon(), Vector2{30, 45}) && !contains(sprite.polygon(), Vector2{31, 45}),
            "Collapsed sprite polygon failed.");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
