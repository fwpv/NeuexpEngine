#include "transform.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float actual, float expected, float tolerance = 0.0001F)
{
    return std::isfinite(actual) && std::abs(actual - expected) <= tolerance;
}

bool near(Engine::Vector2 actual, Engine::Vector2 expected, float tolerance = 0.0001F)
{
    return near(actual.x, expected.x, tolerance) && near(actual.y, expected.y, tolerance);
}

bool near(const Engine::Affine2& actual, const Engine::Affine2& expected)
{
    return near(actual.m00, expected.m00) && near(actual.m01, expected.m01) && near(actual.m02, expected.m02)
        && near(actual.m10, expected.m10) && near(actual.m11, expected.m11) && near(actual.m12, expected.m12);
}
}

int main()
{
    try {
        using namespace Engine;
        const Vector2 point = { 3, -7 };
        const Affine2 identity;
        require(identity.transformPoint(point) == point && identity.transformVector(point) == point, "Identity failed.");
        require(near(Transform2{}.toAffine(), identity), "Default transform is not identity.");

        Transform2 transform;
        transform.position = { 10, 20 };
        transform.origin = { 2, 3 };
        transform.scale = { 2, 3 };
        transform.rotation = Pi / 2;
        const Affine2 matrix = transform.toAffine();
        require(near(matrix.transformPoint(transform.origin), transform.position), "Origin does not map to position.");
        require(near(matrix.transformPoint({ 3, 5 }), { 4, 22 }), "Scale/rotate/translate order failed.");
        require(near(matrix.transformVector({ 1, 2 }), { -6, 2 }), "Vector was translated or origin was applied.");

        Affine2 inverse;
        require(matrix.inverse(inverse), "Inverse failed.");
        require(near(inverse.transformPoint({ 4, 22 }), { 3, 5 }), "Inverse point mapping failed.");
        const Rect localBox = { 2, 3, 1, 2 };
        require(contains(localBox, inverse.transformPoint({ 7, 21 })), "Local hit test rejected an interior world point.");
        require(!contains(localBox, inverse.transformPoint({ 3, 21 })), "Local hit test accepted an exterior world point.");
        require(near(inverse.transformVector({ -6, 2 }), { 1, 2 }), "Inverse vector mapping failed.");
        require(near(matrix * inverse, identity) && near(inverse * matrix, identity), "Inverse composition failed.");
        Affine2 aliased = matrix;
        require(aliased.inverse(aliased) && near(aliased, inverse), "Aliased inversion failed.");

        Transform2 parent;
        parent.position = { 7, -11 };
        parent.scale = { 2, 0.5F };
        Transform2 child;
        child.position = { -3, 4 };
        child.rotation = Pi / 4;
        const Affine2 composed = parent.toAffine() * child.toAffine();
        require(near(composed.transformPoint(point), parent.toAffine().transformPoint(child.toAffine().transformPoint(point))),
            "Composition order failed.");
        // Неравномерный масштаб родителя и поворот потомка дают неперпендикулярные оси.
        require(std::abs(dot(composed.transformVector({ 1, 0 }), composed.transformVector({ 0, 1 }))) > 1,
            "Composition lost shear.");
        require(composed.inverse(inverse) && near(inverse.transformPoint(composed.transformPoint(point)), point),
            "Sheared inverse round trip failed.");
        Affine2 compound = parent.toAffine();
        require(&(compound *= child.toAffine()) == &compound && near(compound, composed), "Compound composition failed.");
        compound = matrix;
        compound *= compound;
        require(near(compound, matrix * matrix), "Self composition failed.");

        const std::array<Vector2, 3> scales = {{ { -2, 3 }, { 0.01F, -0.02F }, { 4, 0.5F } }};
        const std::array<float, 4> angles = { 0, 0.2F, -1.7F, Pi };
        for (Vector2 scale : scales) {
            for (float angle : angles) {
                transform.scale = scale;
                transform.rotation = angle;
                const Affine2 forward = transform.toAffine();
                require(forward.inverse(inverse), "Valid transform rejected.");
                // При масштабе 0.01 ошибка округления мировых float-координат усиливается в сто раз.
                require(near(inverse.transformPoint(forward.transformPoint(point)), point, 0.001F), "Transform round trip failed.");
            }
        }

        transform = {};
        transform.scale = { -2, 3 };
        transform.position = { 10, 20 };
        const Rect reflected = transform.toAffine().transformBounds({ 1, 2, 4, 5 });
        require(reflected.position == Vector2{ 0, 26 } && reflected.size == Size2{ 8, 15 }, "Reflected bounds failed.");
        transform = {};
        transform.rotation = Pi / 2;
        const Rect turned = transform.toAffine().transformBounds({ 0, 0, 4, 2 });
        require(near(turned.position, { -2, 0 }) && near(turned.size.width, 2) && near(turned.size.height, 4),
            "Rotated bounds failed.");
        const Affine2 shear = { 1, 2, 3, 0, 1, 4 };
        const Rect sheared = shear.transformBounds({ 0, 0, 2, 3 });
        require(sheared.position == Vector2{ 3, 4 } && sheared.size == Size2{ 8, 3 }, "Sheared bounds failed.");
        const Rect zero = shear.transformBounds({ 1, 2, 0, 0 });
        require(zero.position == shear.transformPoint({ 1, 2 }) && zero.size == Size2{}, "Zero bounds failed.");

        const Affine2 saved = inverse;
        transform.scale = { 0, 1 };
        require(!transform.toAffine().inverse(inverse) && near(inverse, saved),
            "Zero scale did not preserve output on failure.");
        const Affine2 singular = { 1, 2, 3, 2, 4, 5 };
        require(!singular.inverse(inverse), "Singular affine accepted.");
        const Affine2 smallScale = { 1.0e-8F, 0, 0, 0, 1.0e-8F, 0 };
        require(smallScale.inverse(inverse),
            "Small invertible scale rejected by an absolute epsilon.");
        const Affine2 tinyScale = { std::numeric_limits<float>::min() / 8, 0, 0, 0, 1, 0 };
        const Affine2 previous = inverse;
        require(!tinyScale.inverse(inverse) && near(inverse, previous),
            "Unrepresentable inverse accepted.");
        require(identity.inverse(inverse) && near(inverse, identity),
            "Successful inversion did not clear an earlier error.");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
