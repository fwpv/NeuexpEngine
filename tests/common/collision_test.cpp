#include "collision/query.h"
#include "collision/shape.h"
#include "core/geometry.h"
#include "core/math2d.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
constexpr float Tolerance = 1e-4F;

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool closeTo(float first, float second, float tolerance = Tolerance)
{
    return std::fabs(first - second) <= tolerance;
}

bool closeTo(NeuexpEngine::Vector2 first, NeuexpEngine::Vector2 second, float tolerance = Tolerance)
{
    return closeTo(first.x, second.x, tolerance) && closeTo(first.y, second.y, tolerance);
}

// Сцену и физику этот модуль пока не знает, поэтому сдвинутая форма собирается заново.
NeuexpEngine::CollisionShape moved(const NeuexpEngine::CollisionShape& shape, NeuexpEngine::Vector2 offset)
{
    using NeuexpEngine::CollisionShape;
    if (shape.kind() == CollisionShape::Kind::Circle) {
        NeuexpEngine::Circle circle = shape.circle();
        circle.center += offset;
        return circle;
    }
    if (shape.kind() == CollisionShape::Kind::Rect) {
        NeuexpEngine::Rect rectangle = shape.rect();
        rectangle.position += offset;
        return rectangle;
    }
    NeuexpEngine::Box box = shape.box();
    box.center += offset;
    return box;
}

// Контракт манифолда: сдвиг второй формы на normal * depth должен убрать проникновение.
void requireManifoldContract(const NeuexpEngine::CollisionShape& first,
    const NeuexpEngine::CollisionShape& second, const char* message)
{
    using namespace NeuexpEngine;
    Manifold manifold;
    require(collide(first, second, manifold), message);
    require(manifold.depth >= 0, message);
    require(closeTo(length(manifold.normal), 1.0F), message);
    Manifold separated;
    if (collide(first, moved(second, manifold.normal * manifold.depth), separated)) {
        require(separated.depth <= 1e-3F, message);
    }
}
}

int main()
{
    try {
        using namespace NeuexpEngine;

        const CollisionShape circle = Circle{ { 10, 10 }, 5 };
        const CollisionShape rectangle = Rect{ 0, 0, 10, 10 };
        const CollisionShape box = Box{ { 0, 0 }, { 10, 10 }, degreesToRadians(45) };
        require(circle.kind() == CollisionShape::Kind::Circle, "Circle kind failed.");
        require(rectangle.kind() == CollisionShape::Kind::Rect, "Rectangle kind failed.");
        require(box.kind() == CollisionShape::Kind::Box, "Box kind failed.");
        require(bounds(circle) == Rect{ 5, 5, 10, 10 }, "Circle bounds failed.");
        require(bounds(rectangle) == Rect{ 0, 0, 10, 10 }, "Rectangle bounds failed.");
        require(closeTo(bounds(box).position, { -7.0711F, -7.0711F })
            && closeTo(bounds(box).size.width, 14.1421F), "Box bounds failed.");
        require(contains(circle, { 12, 10 }) && !contains(circle, { 16, 10 }), "Circle point containment failed.");
        require(contains(rectangle, { 0, 0 }) && !contains(rectangle, { 11, 5 }),
            "Rectangle point containment failed.");
        require(contains(box, { 3, 3 }) && !contains(box, { 4, 4 }), "Box point containment failed.");

        Manifold manifold;
        const CollisionShape firstCircle = Circle{ { 0, 0 }, 5 };
        const CollisionShape secondCircle = Circle{ { 8, 0 }, 5 };
        require(collide(firstCircle, secondCircle, manifold)
            && closeTo(manifold.normal, { 1, 0 }) && closeTo(manifold.depth, 2), "Circle overlap failed.");
        require(collide(firstCircle, CollisionShape{Circle{ { 10, 0 }, 5 }}, manifold)
            && manifold.depth == 0 && closeTo(manifold.normal, { 1, 0 }), "Circle contact failed.");
        manifold = Manifold{};
        require(!collide(firstCircle, CollisionShape{Circle{ { 10.5F, 0 }, 5 }}, manifold)
            && manifold.normal == Vector2{ 1, 0 } && manifold.depth == 0, "Separated circles changed the manifold.");
        require(collide(CollisionShape{Circle{ { 0, 0 }, 3 }}, CollisionShape{Circle{ { 0, 0 }, 4 }}, manifold)
            && closeTo(manifold.normal, { 1, 0 }) && closeTo(manifold.depth, 7), "Coincident circles failed.");
        require(collide(CollisionShape{Circle{ { 0, 0 }, 4 }}, CollisionShape{Circle{ { 1, 0 }, 2 }}, manifold)
            && closeTo(manifold.normal, { 1, 0 }) && closeTo(manifold.depth, 5), "Contained circles failed.");
        require(collide(CollisionShape{Circle{ { 1, 0 }, 2 }}, CollisionShape{Circle{ { 0, 0 }, 4 }}, manifold)
            && closeTo(manifold.normal, { -1, 0 }) && closeTo(manifold.depth, 5),
            "Reversed circle normal failed.");

        require(collide(CollisionShape{Circle{ { 13, 5 }, 4 }}, rectangle, manifold)
            && closeTo(manifold.normal, { -1, 0 }) && closeTo(manifold.depth, 1), "Circle/rectangle edge failed.");
        require(collide(rectangle, CollisionShape{Circle{ { 13, 5 }, 4 }}, manifold)
            && closeTo(manifold.normal, { 1, 0 }) && closeTo(manifold.depth, 1),
            "Reversed circle/rectangle normal failed.");
        require(collide(CollisionShape{Circle{ { 15, 5 }, 5 }}, rectangle, manifold)
            && closeTo(manifold.depth, 0) && closeTo(manifold.normal, { -1, 0 }), "Circle/rectangle contact failed.");
        require(collide(CollisionShape{Circle{ { 13, 13 }, 5 }}, rectangle, manifold)
            && closeTo(manifold.normal, { -0.7071F, -0.7071F }) && closeTo(manifold.depth, 0.7574F),
            "Circle/rectangle corner failed.");
        require(collide(CollisionShape{Circle{ { 2, 5 }, 2 }}, rectangle, manifold)
            && closeTo(manifold.normal, { 1, 0 }) && closeTo(manifold.depth, 4),
            "Circle inside rectangle failed.");
        require(collide(rectangle, CollisionShape{Circle{ { 2, 5 }, 2 }}, manifold)
            && closeTo(manifold.normal, { -1, 0 }) && closeTo(manifold.depth, 4),
            "Reversed circle inside rectangle failed.");

        require(collide(rectangle, CollisionShape{Rect{ 8, 2, 10, 10 }}, manifold)
            && closeTo(manifold.normal, { 1, 0 }) && closeTo(manifold.depth, 2), "Rectangle overlap failed.");
        require(collide(rectangle, CollisionShape{Rect{ 2, 8, 10, 10 }}, manifold)
            && closeTo(manifold.normal, { 0, 1 }) && closeTo(manifold.depth, 2),
            "Rectangle vertical overlap failed.");
        require(collide(rectangle, CollisionShape{Rect{ 10, 0, 10, 10 }}, manifold)
            && manifold.depth == 0 && closeTo(manifold.normal, { 1, 0 }), "Rectangle contact failed.");
        require(collide(rectangle, CollisionShape{Rect{ 2, 2, 2, 2 }}, manifold)
            && closeTo(manifold.normal, { -1, 0 }) && closeTo(manifold.depth, 4),
            "Rectangle containment failed.");
        manifold = Manifold{};
        require(!collide(rectangle, CollisionShape{Rect{ 10.5F, 0, 10, 10 }}, manifold)
            && manifold.depth == 0, "Separated rectangles changed the manifold.");

        const CollisionShape smallBox = Box{ { 0, 0 }, { 4, 4 }, 0 };
        require(collide(smallBox, CollisionShape{Box{ { 3, 0 }, { 4, 4 }, degreesToRadians(45) }}, manifold)
            && closeTo(manifold.normal, { 1, 0 }) && closeTo(manifold.depth, 1.8284F),
            "Turned box overlap failed.");
        require(collide(CollisionShape{Box{ { 0, 0 }, { 4, 4 }, degreesToRadians(45) }}, smallBox, manifold)
            && closeTo(manifold.normal, { 0.7071F, 0.7071F }) && closeTo(manifold.depth, 4.8284F),
            "Turned box around box failed.");
        manifold = Manifold{};
        require(!collide(smallBox, CollisionShape{Box{ { 10, 0 }, { 4, 4 }, degreesToRadians(45) }}, manifold),
            "Separated turned boxes intersect.");
        require(collide(rectangle, CollisionShape{Box{ { 0, 0 }, { 10, 10 }, degreesToRadians(45) }}, manifold)
            && closeTo(manifold.normal, { -0.7071F, -0.7071F }) && closeTo(manifold.depth, 5),
            "Turned box inside rectangle failed.");

        require(collide(CollisionShape{Circle{ { 8, 0 }, 2 }}, box, manifold)
            && closeTo(manifold.normal, { -1, 0 }) && closeTo(manifold.depth, 1.0711F),
            "Circle/turned box corner failed.");
        require(collide(box, CollisionShape{Circle{ { 8, 0 }, 2 }}, manifold)
            && closeTo(manifold.normal, { 1, 0 }) && closeTo(manifold.depth, 1.0711F),
            "Reversed circle/turned box normal failed.");

        require(collide(CollisionShape{Rect{}}, CollisionShape{Rect{}}, manifold) && manifold.depth == 0,
            "Zero rectangles do not touch.");
        require(!collide(CollisionShape{Rect{}}, CollisionShape{Rect{ 1, 0, 0, 0 }}, manifold),
            "Separated zero rectangles intersect.");
        require(collide(CollisionShape{Circle{}}, CollisionShape{Rect{}}, manifold) && manifold.depth == 0,
            "Zero circle and rectangle do not touch.");
        require(collide(CollisionShape{Circle{ { 0, 0 }, 2 }}, CollisionShape{Rect{}}, manifold)
            && closeTo(manifold.normal, { 1, 0 }) && closeTo(manifold.depth, 2),
            "Circle around zero rectangle failed.");
        require(collide(CollisionShape{Circle{ { 1, 0 }, 1 }}, CollisionShape{Rect{}}, manifold)
            && closeTo(manifold.normal, { -1, 0 }) && closeTo(manifold.depth, 0),
            "Circle touching zero rectangle failed.");

        require(overlaps(firstCircle, secondCircle) && overlaps(secondCircle, firstCircle),
            "Overlap is not symmetric.");
        require(!overlaps(circle, CollisionShape{Circle{ { 21, 10 }, 5 }}) && overlaps(rectangle, box),
            "Overlap disagrees with collision.");
        requireManifoldContract(firstCircle, secondCircle, "Circle manifold contract failed.");
        requireManifoldContract(CollisionShape{Circle{ { 13, 13 }, 5 }}, rectangle, "Corner manifold contract failed.");
        requireManifoldContract(rectangle, CollisionShape{Rect{ 2, 2, 2, 2 }}, "Containment manifold contract failed.");
        requireManifoldContract(smallBox, CollisionShape{Box{ { 3, 0 }, { 4, 4 }, degreesToRadians(45) }},
            "Turned box manifold contract failed.");
        requireManifoldContract(CollisionShape{Circle{ { 8, 0 }, 2 }}, box, "Turned box circle contract failed.");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
