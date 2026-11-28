#include "collision/query.h"
#include "collision/shape.h"
#include "core/geometry.h"
#include "core/math2d.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
constexpr float Tolerance = 1e-3F;

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

// Нормаль попадания должна быть единичной и направленной против движения.
void requireHitNormal(const NeuexpEngine::SegmentHit& hit, NeuexpEngine::Vector2 direction, const char* message)
{
    require(closeTo(NeuexpEngine::length(hit.normal), 1.0F), message);
    require(NeuexpEngine::dot(hit.normal, direction) < 0, message);
}
}

int main()
{
    try {
        using namespace NeuexpEngine;

        const CollisionShape circle = Circle{Vector2{0, 0}, 5};
        const CollisionShape smallCircle = Circle{Vector2{10, 0}, 3};
        const CollisionShape rectangle = Rect{0, 0, 10, 10};
        const CollisionShape distantRectangle = Rect{20, 5, 10, 10};
        const CollisionShape diagonalRectangle = Rect{20, 20, 10, 10};

        require(closeTo(distance(circle, smallCircle), 2), "Circle distance failed.");
        require(distance(circle, CollisionShape{Circle{Vector2{8, 0}, 5}}) == 0, "Overlapping circle distance is not zero.");
        require(distance(circle, CollisionShape{Circle{Vector2{10, 0}, 5}}) == 0, "Touching circle distance is not zero.");
        require(closeTo(distance(CollisionShape{Circle{Vector2{15, 5}, 2}}, rectangle), 3),
            "Circle/rectangle edge distance failed.");
        require(closeTo(distance(CollisionShape{Circle{Vector2{20, 20}, 1}}, rectangle), 13.1421F),
            "Circle/rectangle corner distance failed.");
        require(distance(CollisionShape{Circle{Vector2{2, 5}, 2}}, rectangle) == 0,
            "Circle inside rectangle distance is not zero.");
        require(closeTo(distance(rectangle, distantRectangle), 10), "Rectangle distance failed.");
        require(closeTo(distance(rectangle, diagonalRectangle), 14.1421F), "Rectangle corner distance failed.");
        require(closeTo(distance(CollisionShape{Box{Vector2{0, 0}, Size2{4, 4}, 0}},
                         CollisionShape{Box{Vector2{10, 0}, Size2{4, 4}, degreesToRadians(45)}}), 5.1716F),
            "Turned box distance failed.");
        require(closeTo(distance(CollisionShape{Circle{Vector2{10, 0}, 1}},
                         CollisionShape{Box{Vector2{0, 0}, Size2{4, 4}, degreesToRadians(45)}}), 6.1716F),
            "Circle/turned box distance failed.");
        require(closeTo(distance(CollisionShape{Rect{}}, CollisionShape{Rect{5, 0, 0, 0}}), 5),
            "Zero rectangle distance failed.");
        require(closeTo(distance(rectangle, CollisionShape{Rect{12, 8, 4, 4}}),
            distance(CollisionShape{Rect{12, 8, 4, 4}}, rectangle)), "Distance is not symmetric.");
        require(closeTo(distance(rectangle, rectangle), 0), "Same shape distance is not zero.");

        SegmentHit hit;
        require(castSegment(circle, Vector2{-10, 0}, Vector2{10, 0}, hit)
            && closeTo(hit.point, Vector2{-5, 0}) && closeTo(hit.normal, Vector2{-1, 0}) && closeTo(hit.fraction, 0.25F)
            && closeTo(hit.point, Vector2{-10, 0} + Vector2{20, 0} * hit.fraction), "Circle hit failed.");
        require(castSegment(circle, Vector2{10, 0}, Vector2{-10, 0}, hit)
            && closeTo(hit.point, Vector2{5, 0}) && closeTo(hit.normal, Vector2{1, 0}) && closeTo(hit.fraction, 0.25F),
            "Reversed circle hit failed.");
        require(castSegment(circle, Vector2{-10, 5}, Vector2{10, 5}, hit)
            && closeTo(hit.point, Vector2{0, 5}) && closeTo(hit.normal, Vector2{0, 1}) && closeTo(hit.fraction, 0.5F),
            "Tangent circle hit failed.");
        hit = SegmentHit{};
        require(!castSegment(circle, Vector2{-10, 6}, Vector2{10, 6}, hit) && hit.fraction == 0 && hit.point == Vector2{},
            "Missed circle hit changed the result.");
        require(castSegment(circle, Vector2{1, 0}, Vector2{10, 0}, hit)
            && closeTo(hit.point, Vector2{1, 0}) && closeTo(hit.normal, Vector2{1, 0}) && hit.fraction == 0,
            "Circle started inside failed.");
        require(castSegment(circle, Vector2{5, 0}, Vector2{10, 0}, hit)
            && closeTo(hit.point, Vector2{5, 0}) && closeTo(hit.normal, Vector2{1, 0}) && hit.fraction == 0,
            "Circle started on the boundary failed.");
        require(!castSegment(circle, Vector2{-10, 0}, Vector2{-6, 0}, hit), "Short segment reached the circle.");
        require(!castSegment(circle, Vector2{10, 10}, Vector2{10, 10}, hit), "Point outside the circle was hit.");
        require(castSegment(circle, Vector2{1, 1}, Vector2{1, 1}, hit) && closeTo(hit.point, Vector2{1, 1})
            && closeTo(hit.normal, Vector2{0.7071F, 0.7071F}) && hit.fraction == 0, "Point inside the circle failed.");
        require(castSegment(CollisionShape{Circle{}}, Vector2{-5, 0}, Vector2{5, 0}, hit)
            && closeTo(hit.point, Vector2{}) && closeTo(hit.normal, Vector2{1, 0}) && closeTo(hit.fraction, 0.5F),
            "Zero circle hit failed.");

        require(castSegment(rectangle, Vector2{-5, 5}, Vector2{15, 5}, hit)
            && closeTo(hit.point, Vector2{0, 5}) && closeTo(hit.normal, Vector2{-1, 0}) && closeTo(hit.fraction, 0.25F),
            "Rectangle left hit failed.");
        require(castSegment(rectangle, Vector2{15, 5}, Vector2{-5, 5}, hit)
            && closeTo(hit.point, Vector2{10, 5}) && closeTo(hit.normal, Vector2{1, 0}) && closeTo(hit.fraction, 0.25F),
            "Rectangle right hit failed.");
        require(castSegment(rectangle, Vector2{5, -5}, Vector2{5, 15}, hit)
            && closeTo(hit.point, Vector2{5, 0}) && closeTo(hit.normal, Vector2{0, -1}) && closeTo(hit.fraction, 0.25F),
            "Rectangle top hit failed.");
        require(castSegment(rectangle, Vector2{-5, -5}, Vector2{15, 15}, hit)
            && closeTo(hit.point, Vector2{0, 0}) && closeTo(hit.normal, Vector2{-1, 0}) && closeTo(hit.fraction, 0.25F),
            "Rectangle corner hit failed.");
        require(castSegment(rectangle, Vector2{5, 1}, Vector2{15, 1}, hit)
            && closeTo(hit.point, Vector2{5, 1}) && closeTo(hit.normal, Vector2{0, -1}) && hit.fraction == 0,
            "Rectangle started inside failed.");
        hit = SegmentHit{};
        require(!castSegment(rectangle, Vector2{5, -5}, Vector2{5, -1}, hit) && hit.point == Vector2{},
            "Short segment reached the rectangle.");
        require(castSegment(CollisionShape{Rect{0, 0, 0, 10}}, Vector2{-5, 5}, Vector2{5, 5}, hit)
            && closeTo(hit.point, Vector2{0, 5}) && closeTo(hit.normal, Vector2{-1, 0}) && closeTo(hit.fraction, 0.5F),
            "Zero width rectangle hit failed.");

        const CollisionShape turned = Box{Vector2{0, 0}, Size2{10, 10}, degreesToRadians(45)};
        require(castSegment(turned, Vector2{-10, 0}, Vector2{-6, 0}, hit)
            && closeTo(hit.point, Vector2{-7.0711F, 0}) && closeTo(hit.fraction, 0.7322F),
            "Turned box hit failed.");
        requireHitNormal(hit, Vector2{4, 0}, "Turned box hit normal failed.");
        hit = SegmentHit{};
        require(!castSegment(turned, Vector2{-10, 4}, Vector2{-8, 4}, hit) && hit.fraction == 0,
            "Missed turned box hit changed the result.");
        require(castSegment(rectangle, Vector2{15, 5}, Vector2{5, 5}, hit)
            && closeTo(hit.point, Vector2{10, 5}) && closeTo(hit.normal, Vector2{1, 0}),
            "Rectangle hit from the right side failed.");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
