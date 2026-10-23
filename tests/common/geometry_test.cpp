#include "geometry.h"

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
        using namespace Engine;
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
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
