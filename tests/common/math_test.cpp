#include "math2d.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float actual, float expected)
{
    return std::isfinite(actual) && std::abs(actual - expected) <= 0.00001F;
}

bool near(NeuexpEngine::Vector2 actual, NeuexpEngine::Vector2 expected)
{
    return near(actual.x, expected.x) && near(actual.y, expected.y);
}
}

int main()
{
    try {
        using namespace NeuexpEngine;
        constexpr Vector2 velocity = { 12, -8 };
        constexpr Vector2 nextPosition = Vector2{ 1, 2 } + velocity * 0.25F;
        static_assert(nextPosition == Vector2{ 4, 0 });
        static_assert(cross({ 1, 0 }, { 0, 1 }) == 1);

        require(Vector2{} == Vector2{ 0, 0 }, "Default vector is not zero.");
        Vector2 value = { 3, -4 };
        require(+value == value && -value == Vector2{ -3, 4 }, "Unary vector operators failed.");
        value += { 1, 2 };
        value -= { 2, 1 };
        value *= 3;
        value /= 2;
        require(value == Vector2{ 3, -4.5F }, "Compound vector arithmetic failed.");
        require(Vector2{ 3, 4 } / 2 == Vector2{ 1.5F, 2 }, "Vector division failed.");
        require(2 * Vector2{ 3, 4 } == Vector2{ 3, 4 } * 2, "Scalar multiplication is not symmetric.");
        require(Vector2{ 1, 2 } != Vector2{ 1, 3 }, "Vector inequality failed.");
        require(dot({ 2, -3 }, { 4, 5 }) == -7, "Dot product failed.");
        require(cross({ 2, -3 }, { 4, 5 }) == 22, "Cross product failed.");
        require(cross({ 4, 5 }, { 2, -3 }) == -22, "Cross product orientation failed.");
        require(lengthSquared({ 3, 4 }) == 25 && near(length({ 3, 4 }), 5), "Vector length failed.");
        require(near(distance({ -1, 2 }, { 2, 6 }), 5), "Distance failed.");
        require(normalized({}) == Vector2{}, "Zero normalization failed.");
        require(near(normalized({ 3, -4 }), { 0.6F, -0.8F }), "Normalization failed.");
        const float maximum = std::numeric_limits<float>::max();
        require(near(length(normalized({ maximum, maximum })), 1), "Large direction normalization failed.");
        require(near(length(normalized({ 1.0e-30F, 0 })), 1), "Small direction normalization failed.");
        require(near(rotated({ 2, 0 }, Pi / 2), { 0, 2 }), "Positive rotation failed.");
        require(near(rotated({ 2, 0 }, -Pi / 2), { 0, -2 }), "Negative rotation failed.");
        require(near(rotated(rotated({ 3, -7 }, 0.7F), -0.7F), { 3, -7 }), "Rotation round trip failed.");
        require(near(degreesToRadians(180), Pi) && near(radiansToDegrees(Pi / 2), 90), "Angle conversion failed.");
        require(lerp({ 2, 4 }, { 6, 8 }, 0.25F) == Vector2{ 3, 5 }, "Interpolation failed.");
        require(lerp({ 2, 4 }, { 6, 8 }, 2) == Vector2{ 10, 12 }, "Extrapolation failed.");

        Size2 size = { 640, 480 };
        size *= 0.5F;
        size /= 2;
        require(size == Size2{ 160, 120 } && Size2{} == Size2{ 0, 0 }, "Size arithmetic failed.");
        require(2 * size == size * 2 && size / 2 == Size2{ 80, 60 }, "Size scaling failed.");
        require(size != Size2{ 160, 121 }, "Size inequality failed.");
        require(Color{} == Color{ 1, 1, 1, 1 }, "Default color failed.");
        require(Color{ 0.2F, 0.3F, 0.4F }.alpha == 1, "RGB color is not opaque.");
        require(Color{ 1, 0, 0, 0.5F } != Color{ 1, 0, 0 }, "Color alpha comparison failed.");

        Vector2 copied = value;
        copied.x = 99;
        require(value.x == 3, "Copied vector shares state.");
        copied = value;
        Vector2 moved = std::move(copied);
        require(moved == value, "Vector value semantics failed.");
        Color color;
        color = Color{ 0, 0.5F, 1, 0.25F };
        require(color.green == 0.5F && color.alpha == 0.25F, "Color assignment failed.");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
