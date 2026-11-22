#include "core/geometry.h"
#include "core/math2d.h"

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

// Повороты считаются через sin/cos, поэтому превратить проверку в точное равенство нельзя.
bool closeTo(NeuexpEngine::Vector2 first, NeuexpEngine::Vector2 second, float tolerance = 1e-4F)
{
    return std::fabs(first.x - second.x) <= tolerance && std::fabs(first.y - second.y) <= tolerance;
}
}

int main()
{
    try {
        using namespace NeuexpEngine;
        const Rect rectangle = { -2, -3, 8, 6 };
        require(rectangle == Rect{ { -2, -3 }, { 8, 6 } } && rectangle != Rect{ -2, -3, 8, 7 },
            "Rectangle value comparison failed.");
        require(rectangle.center() == Vector2{ 2, 0 }, "Rectangle center failed.");
        require(contains(rectangle, { 0, 0 }), "Rectangle does not contain an interior point.");
        require(contains(rectangle, { -2, -3 }) && contains(rectangle, { 6, 3 }), "Rectangle boundary excluded.");
        require(!contains(rectangle, { -2.01F, 0 }) && !contains(rectangle, { 0, 3.01F }), "Exterior point included.");
        require(closestPoint(rectangle, { 1, 2 }) == Vector2{ 1, 2 }, "Interior closest point changed.");
        require(closestPoint(rectangle, { -4, 0 }) == Vector2{ -2, 0 }, "Left closest point failed.");
        require(closestPoint(rectangle, { 8, 0 }) == Vector2{ 6, 0 }, "Right closest point failed.");
        require(closestPoint(rectangle, { 0, -5 }) == Vector2{ 0, -3 }, "Lower closest point failed.");
        require(closestPoint(rectangle, { 0, 5 }) == Vector2{ 0, 3 }, "Upper closest point failed.");
        require(closestPoint(rectangle, { 9, -8 }) == Vector2{ 6, -3 }, "Corner closest point failed.");
        require(closestPoint(rectangle, { 6, 3 }) == Vector2{ 6, 3 }, "Boundary closest point changed.");
        require(closestPoint(Rect{}, { 2, -3 }) == Vector2{}, "Zero rectangle closest point failed.");
        require(closestPoint({ 1, 2, 0, 4 }, { -3, 9 }) == Vector2{ 1, 6 }, "Line rectangle closest point failed.");

        const Rect overlap = { { 4, 1 }, { 5, 4 } };
        require(intersects(rectangle, overlap) && intersects(overlap, rectangle), "Rectangle overlap failed.");
        require(intersects(rectangle, { 6, 3, 1, 1 }), "Rectangle corner contact failed.");
        require(!intersects(rectangle, { 6.01F, -3, 1, 6 }), "Separated rectangles intersect.");
        require(intersects(rectangle, { 0, 0, 1, 1 }), "Contained rectangles do not intersect.");
        Rect clipped;
        require(intersection(rectangle, overlap, clipped)
            && clipped.position == Vector2{ 4, 1 } && clipped.size == Size2{ 2, 2 }, "Rectangle clipping failed.");
        require(intersection(rectangle, { 6, -3, 2, 6 }, clipped)
            && clipped.position == Vector2{ 6, -3 } && clipped.size == Size2{ 0, 6 }, "Edge clipping failed.");
        const Rect saved = clipped;
        require(!intersection(rectangle, { 20, 20, 1, 1 }, clipped)
            && clipped.position == saved.position && clipped.size == saved.size, "Failed clipping changed output.");
        Rect aliased = rectangle;
        require(intersection(aliased, overlap, aliased) && aliased.size == Size2{ 2, 2 }, "Aliased clipping failed.");

        const Circle circle = { { -1, 2 }, 5 };
        require(circle == Circle{ { -1, 2 }, 5 } && circle != Circle{ { -1, 2 }, 6 }, "Circle value comparison failed.");
        require(contains(circle, { 2, 6 }), "Circle boundary excluded.");
        require(contains(circle, circle.center) && !contains(circle, { 2, 6.01F }), "Circle containment failed.");
        const Circle touching = { { 9, 2 }, 5 };
        require(intersects(circle, touching) && intersects(touching, circle), "Circle contact failed.");
        require(!intersects(circle, Circle{ { 9.01F, 2 }, 5 }), "Separated circles intersect.");
        require(intersects(circle, Circle{ { -1, 2 }, 1 }), "Contained circles do not intersect.");

        const Rect box = { 0, 0, 4, 4 };
        const Circle cornerContact = { { 7, 8 }, 5 };
        require(intersects(box, cornerContact) && intersects(cornerContact, box), "Circle/rectangle corner contact failed.");
        require(!intersects(box, Circle{ { 7, 8.01F }, 5 }), "Circle/rectangle corner gap failed.");
        require(intersects(box, Circle{ { 6, 2 }, 2 }), "Circle/rectangle edge contact failed.");
        require(intersects(box, Circle{ { 2, 2 }, 0.5F }), "Rectangle/circle containment failed.");
        require(intersects(box, Circle{ { 2, 2 }, 20 }), "Circle/rectangle containment failed.");

        require(contains(Rect{}, {}) && intersects(Rect{}, Rect{}), "Zero rectangle failed.");
        require(contains(Circle{}, {}) && intersects(Circle{}, Circle{}), "Zero circle failed.");
        require(!contains(Circle{}, { 0.01F, 0 }), "Zero circle contains a different point.");
        require(intersects(Rect{}, Circle{}) && !intersects(Rect{}, Circle{ { 1, 0 }, 0 }), "Degenerate mixed contact failed.");

        const Box flat = { { 5, 5 }, { 10, 10 }, 0 };
        require(flat == Box{ { 5, 5 }, { 10, 10 }, 0 } && flat != Box{ { 5, 5 }, { 10, 10 }, 1 },
            "Box value comparison failed.");
        require(flat.corners() == Rect{ 0, 0, 10, 10 }.corners(), "Flat box corners differ from rectangle.");
        require(bounds(flat) == Rect{ 0, 0, 10, 10 }, "Flat box bounds failed.");
        require(contains(flat, { 0, 0 }) && contains(flat, { 10, 10 }) && !contains(flat, { 10.01F, 5 }),
            "Flat box containment failed.");
        require(closestPoint(flat, { 13, 5 }) == Vector2{ 10, 5 } && closestPoint(flat, { 2, 3 }) == Vector2{ 2, 3 },
            "Flat box closest point failed.");

        const Box turned = { { 0, 0 }, { 4, 4 }, degreesToRadians(45) };
        const float diagonal = 2.8284271F;
        require(closeTo(turned.corners()[0], { 0, -diagonal }) && closeTo(turned.corners()[1], { diagonal, 0 })
            && closeTo(turned.corners()[2], { 0, diagonal }) && closeTo(turned.corners()[3], { -diagonal, 0 }),
            "Turned box corners failed.");
        require(closeTo(bounds(turned).position, { -diagonal, -diagonal })
            && closeTo(Vector2{ bounds(turned).size.width, bounds(turned).size.height }, { diagonal * 2, diagonal * 2 }),
            "Turned box bounds failed.");
        require(contains(turned, {}) && contains(turned, { 2.5F, 0 }) && !contains(turned, { 3, 0 }),
            "Turned box containment failed.");
        require(closeTo(closestPoint(turned, { 5, 0 }), { diagonal, 0 })
            && closeTo(closestPoint(turned, { 0, 0 }), {}), "Turned box closest point failed.");
        require(contains(Box{}, {}) && bounds(Box{}) == Rect{}, "Zero box failed.");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
