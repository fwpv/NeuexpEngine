#include "query.h"
#include "core/math2d.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace NeuexpEngine
{
namespace
{
// Пересечение и расстояние считаются в double: разности координат и произведения
// квадратов во float теряют точность уже на сотнях единиц мира.

// Полуразмеры прямоугольника: используются проверками в его осях.
Size2 halfSizeOf(const Box& box)
{
    return {box.size.width / 2, box.size.height / 2};
}

// Прямоугольник вдоль осей в виде повёрнутого прямоугольника с нулевым поворотом.
// Вызов требует прямоугольного вида формы.
Box boxOf(const CollisionShape& shape)
{
    if (shape.kind() == CollisionShape::Kind::Box) {
        return shape.box();
    }
    const Rect rectangle = shape.rect();
    return {rectangle.center(), rectangle.size, 0};
}

// Нулевое смещение не задаёт направления, поэтому для него принимается (1, 0).
Vector2 radialNormal(Vector2 offset)
{
    const Vector2 normal = normalized(offset);
    return normal == Vector2{} ? Vector2{1, 0} : normal;
}

// Ближайшая грань прямоугольника в его осях: наружная нормаль и расстояние до неё.
// Точка может лежать внутри прямоугольника.
struct FaceHit
{
    Vector2 normal;
    double distance = 0;
};

FaceHit nearestFace(Vector2 localPoint, Size2 halfSize)
{
    const double leftGap = static_cast<double>(localPoint.x) + halfSize.width;
    const double rightGap = static_cast<double>(halfSize.width) - localPoint.x;
    const double upperGap = static_cast<double>(localPoint.y) + halfSize.height;
    const double lowerGap = static_cast<double>(halfSize.height) - localPoint.y;
    FaceHit result = {Vector2{-1, 0}, leftGap};
    if (rightGap < result.distance) {
        result = {Vector2{1, 0}, rightGap};
    }
    if (upperGap < result.distance) {
        result = {Vector2{0, -1}, upperGap};
    }
    if (lowerGap < result.distance) {
        result = {Vector2{0, 1}, lowerGap};
    }
    return result;
}

bool collideCircles(const Circle& first, const Circle& second, Manifold& result)
{
    const double offsetX = static_cast<double>(second.center.x) - first.center.x;
    const double offsetY = static_cast<double>(second.center.y) - first.center.y;
    const double radii = static_cast<double>(first.radius) + second.radius;
    const double offsetSquared = offsetX * offsetX + offsetY * offsetY;
    if (offsetSquared > radii * radii) {
        return false;
    }
    if (offsetSquared == 0) {
        result = {Vector2{1, 0}, static_cast<float>(radii)};
        return true;
    }
    const double offsetLength = std::sqrt(offsetSquared);
    result = {Vector2{static_cast<float>(offsetX / offsetLength),
                  static_cast<float>(offsetY / offsetLength)},
        static_cast<float>(radii - offsetLength)};
    return true;
}

// Пересечение круга с прямоугольником в осях этого прямоугольника.
bool collideCircleRectangle(Vector2 circleCenter, float radius, Size2 halfSize, Manifold& result)
{
    const Vector2 closest{
        std::clamp(circleCenter.x, -halfSize.width, halfSize.width),
        std::clamp(circleCenter.y, -halfSize.height, halfSize.height)};
    const double offsetX = static_cast<double>(closest.x) - circleCenter.x;
    const double offsetY = static_cast<double>(closest.y) - circleCenter.y;
    const double offsetSquared = offsetX * offsetX + offsetY * offsetY;
    const double radiusValue = radius;
    if (offsetSquared > radiusValue * radiusValue) {
        return false;
    }
    if (offsetSquared > 0) {
        const double offsetLength = std::sqrt(offsetSquared);
        result = {Vector2{static_cast<float>(offsetX / offsetLength),
                      static_cast<float>(offsetY / offsetLength)},
            static_cast<float>(radiusValue - offsetLength)};
        return true;
    }
    // Центр круга внутри прямоугольника: выталкивание идёт наружу по ближайшей грани,
    // а нормаль направлена от круга к прямоугольнику, то есть против наружной нормали.
    const FaceHit face = nearestFace(circleCenter, halfSize);
    result = {-face.normal, static_cast<float>(radiusValue + face.distance)};
    return true;
}

bool collideCircleBox(const Circle& circle, const Box& box, Manifold& result)
{
    const Vector2 localCenter = rotated(circle.center - box.center, -box.rotation);
    Manifold local;
    if (!collideCircleRectangle(localCenter, circle.radius, halfSizeOf(box), local)) {
        return false;
    }
    result = {rotated(local.normal, box.rotation), local.depth};
    return true;
}

// Радиус проекции прямоугольника на ось.
double boxRadius(const Box& box, Vector2 axisX, Vector2 axisY, Vector2 axis)
{
    const double projectionX = std::abs(static_cast<double>(dot(axis, axisX)));
    const double projectionY = std::abs(static_cast<double>(dot(axis, axisY)));
    const double halfWidth = static_cast<double>(box.size.width) / 2;
    const double halfHeight = static_cast<double>(box.size.height) / 2;
    return halfWidth * projectionX + halfHeight * projectionY;
}

// Разделяющие оси: две оси каждого прямоугольника. Глубина вдоль оси — наименьший сдвиг
// второй формы в одну из сторон, поэтому вложенный прямоугольник выталкивается за
// ближайшую грань, а не на сумму проекций.
bool collideBoxes(const Box& first, const Box& second, Manifold& result)
{
    const Vector2 firstAxisX = rotated(Vector2{1, 0}, first.rotation);
    const Vector2 firstAxisY = rotated(Vector2{0, 1}, first.rotation);
    const Vector2 secondAxisX = rotated(Vector2{1, 0}, second.rotation);
    const Vector2 secondAxisY = rotated(Vector2{0, 1}, second.rotation);
    const std::array<Vector2, 4> axes = {firstAxisX, firstAxisY, secondAxisX, secondAxisY};
    double bestDepth = 0;
    Vector2 bestNormal{1, 0};
    for (std::size_t index = 0; index < axes.size(); ++index) {
        const Vector2 axis = axes[index];
        const double centerFirst = static_cast<double>(dot(axis, first.center));
        const double centerSecond = static_cast<double>(dot(axis, second.center));
        const double radiusFirst = boxRadius(first, firstAxisX, firstAxisY, axis);
        const double radiusSecond = boxRadius(second, secondAxisX, secondAxisY, axis);
        const double pushPositive = centerFirst + radiusFirst - (centerSecond - radiusSecond);
        const double pushNegative = centerSecond + radiusSecond - (centerFirst - radiusFirst);
        if (pushPositive < 0 || pushNegative < 0) {
            return false;
        }
        double depth = pushPositive;
        Vector2 normal = axis;
        if (pushNegative < pushPositive) {
            depth = pushNegative;
            normal = -axis;
        }
        if (index == 0 || depth < bestDepth) {
            bestDepth = depth;
            bestNormal = normal;
        }
    }
    result = {bestNormal, static_cast<float>(bestDepth)};
    return true;
}

bool detect(const CollisionShape& first, const CollisionShape& second, Manifold& result)
{
    const bool firstIsCircle = first.kind() == CollisionShape::Kind::Circle;
    const bool secondIsCircle = second.kind() == CollisionShape::Kind::Circle;
    if (firstIsCircle && secondIsCircle) {
        return collideCircles(first.circle(), second.circle(), result);
    }
    if (firstIsCircle) {
        return collideCircleBox(first.circle(), boxOf(second), result);
    }
    if (secondIsCircle) {
        Manifold reversed;
        if (!collideCircleBox(second.circle(), boxOf(first), reversed)) {
            return false;
        }
        result = {-reversed.normal, reversed.depth};
        return true;
    }
    return collideBoxes(boxOf(first), boxOf(second), result);
}

// Расстояние от точки до границы прямоугольника; внутри прямоугольника равно нулю.
double pointGap(Vector2 point, const Box& box)
{
    const Vector2 local = rotated(point - box.center, -box.rotation);
    const double halfWidth = static_cast<double>(box.size.width) / 2;
    const double halfHeight = static_cast<double>(box.size.height) / 2;
    const double gapX = std::max(0.0, std::abs(static_cast<double>(local.x)) - halfWidth);
    const double gapY = std::max(0.0, std::abs(static_cast<double>(local.y)) - halfHeight);
    return std::hypot(gapX, gapY);
}

// Кратчайшее расстояние между двумя отрезками.
double segmentDistance(Vector2 firstBegin, Vector2 firstEnd, Vector2 secondBegin, Vector2 secondEnd)
{
    const double firstDirectionX = static_cast<double>(firstEnd.x) - firstBegin.x;
    const double firstDirectionY = static_cast<double>(firstEnd.y) - firstBegin.y;
    const double secondDirectionX = static_cast<double>(secondEnd.x) - secondBegin.x;
    const double secondDirectionY = static_cast<double>(secondEnd.y) - secondBegin.y;
    const double offsetX = static_cast<double>(firstBegin.x) - secondBegin.x;
    const double offsetY = static_cast<double>(firstBegin.y) - secondBegin.y;
    const double firstLengthSquared = firstDirectionX * firstDirectionX + firstDirectionY * firstDirectionY;
    const double secondLengthSquared = secondDirectionX * secondDirectionX + secondDirectionY * secondDirectionY;
    const double secondProjection = secondDirectionX * offsetX + secondDirectionY * offsetY;
    double firstTime = 0;
    double secondTime = 0;
    if (firstLengthSquared == 0 && secondLengthSquared == 0) {
        return std::hypot(offsetX, offsetY);
    }
    if (firstLengthSquared == 0) {
        secondTime = std::clamp(secondProjection / secondLengthSquared, 0.0, 1.0);
    } else {
        const double firstProjection = firstDirectionX * offsetX + firstDirectionY * offsetY;
        if (secondLengthSquared == 0) {
            firstTime = std::clamp(-firstProjection / firstLengthSquared, 0.0, 1.0);
        } else {
            const double mix = firstDirectionX * secondDirectionX + firstDirectionY * secondDirectionY;
            const double denominator = firstLengthSquared * secondLengthSquared - mix * mix;
            if (denominator != 0) {
                const double unclamped = (mix * secondProjection - firstProjection * secondLengthSquared)
                    / denominator;
                firstTime = std::clamp(unclamped, 0.0, 1.0);
            }
            secondTime = (mix * firstTime + secondProjection) / secondLengthSquared;
            if (secondTime < 0) {
                secondTime = 0;
                firstTime = std::clamp(-firstProjection / firstLengthSquared, 0.0, 1.0);
            } else if (secondTime > 1) {
                secondTime = 1;
                firstTime = std::clamp((mix - firstProjection) / firstLengthSquared, 0.0, 1.0);
            }
        }
    }
    const double firstPointX = firstBegin.x + firstDirectionX * firstTime;
    const double firstPointY = firstBegin.y + firstDirectionY * firstTime;
    const double secondPointX = secondBegin.x + secondDirectionX * secondTime;
    const double secondPointY = secondBegin.y + secondDirectionY * secondTime;
    return std::hypot(firstPointX - secondPointX, firstPointY - secondPointY);
}

// Расстояние между границами непересекающихся прямоугольников: минимальное по парам рёбер.
double boxesDistance(const Box& first, const Box& second)
{
    const Quad firstCorners = first.corners();
    const Quad secondCorners = second.corners();
    double best = std::numeric_limits<double>::max();
    for (std::size_t firstIndex = 0; firstIndex < firstCorners.size(); ++firstIndex) {
        const Vector2 firstBegin = firstCorners[firstIndex];
        const Vector2 firstEnd = firstCorners[(firstIndex + 1) % firstCorners.size()];
        for (std::size_t secondIndex = 0; secondIndex < secondCorners.size(); ++secondIndex) {
            const Vector2 secondBegin = secondCorners[secondIndex];
            const Vector2 secondEnd = secondCorners[(secondIndex + 1) % secondCorners.size()];
            best = std::min(best, segmentDistance(firstBegin, firstEnd, secondBegin, secondEnd));
        }
    }
    return best;
}

bool castCircle(const Circle& circle, Vector2 begin, Vector2 end, SegmentHit& result)
{
    const Vector2 direction = end - begin;
    const Vector2 offset = begin - circle.center;
    if (contains(circle, begin)) {
        result = {begin, radialNormal(offset), 0};
        return true;
    }
    const double directionLengthSquared = static_cast<double>(lengthSquared(direction));
    if (directionLengthSquared == 0) {
        return false;
    }
    const double halfProjection = static_cast<double>(dot(offset, direction));
    const double radius = circle.radius;
    const double offsetLengthSquared = static_cast<double>(lengthSquared(offset));
    const double discriminant = halfProjection * halfProjection - directionLengthSquared
        * (offsetLengthSquared - radius * radius);
    if (discriminant < 0) {
        return false;
    }
    const double fraction = (-halfProjection - std::sqrt(discriminant)) / directionLengthSquared;
    if (fraction < 0 || fraction > 1) {
        return false;
    }
    const float hitFraction = static_cast<float>(fraction);
    const Vector2 point = begin + direction * hitFraction;
    result = {point, radialNormal(point - circle.center), hitFraction};
    return true;
}

bool castBox(const Box& box, Vector2 begin, Vector2 end, SegmentHit& result)
{
    const Size2 halfSize = halfSizeOf(box);
    const Vector2 localBegin = rotated(begin - box.center, -box.rotation);
    const Vector2 localEnd = rotated(end - box.center, -box.rotation);
    const std::array<double, 2> lows = {-static_cast<double>(halfSize.width),
        -static_cast<double>(halfSize.height)};
    const std::array<double, 2> highs = {halfSize.width, halfSize.height};
    const std::array<double, 2> starts = {localBegin.x, localBegin.y};
    const std::array<double, 2> finishes = {localEnd.x, localEnd.y};
    double enter = -std::numeric_limits<double>::infinity();
    double leave = std::numeric_limits<double>::infinity();
    std::size_t enterAxis = 0;
    bool enterFromLow = true;
    for (std::size_t axis = 0; axis < starts.size(); ++axis) {
        const double start = starts[axis];
        const double direction = finishes[axis] - start;
        if (direction == 0) {
            if (start < lows[axis] || start > highs[axis]) {
                return false;
            }
            continue;
        }
        double lowTime = (lows[axis] - start) / direction;
        double highTime = (highs[axis] - start) / direction;
        bool fromLow = true;
        if (lowTime > highTime) {
            std::swap(lowTime, highTime);
            fromLow = false;
        }
        if (lowTime > enter) {
            enter = lowTime;
            enterAxis = axis;
            enterFromLow = fromLow;
        }
        if (highTime < leave) {
            leave = highTime;
        }
        if (enter > leave) {
            return false;
        }
    }
    if (enter > 1 || leave < 0) {
        return false;
    }
    if (enter < 0) {
        // Отрезок начинается внутри прямоугольника: попадание в начале с нормалью
        // ближайшей грани. Совпадающие концы приходят сюда же, а точку вне
        // прямоугольника отсекает проверка границ по каждой оси.
        result = {begin, rotated(nearestFace(localBegin, halfSize).normal, box.rotation), 0};
        return true;
    }
    const Vector2 localPoint{
        static_cast<float>(starts[0] + (finishes[0] - starts[0]) * enter),
        static_cast<float>(starts[1] + (finishes[1] - starts[1]) * enter)};
    // Нормаль грани направлена наружу: при входе через малую границу — против оси.
    const float sign = enterFromLow ? -1.0F : 1.0F;
    const Vector2 localNormal = enterAxis == 0 ? Vector2{sign, 0} : Vector2{0, sign};
    result = {box.center + rotated(localPoint, box.rotation), rotated(localNormal, box.rotation),
        static_cast<float>(enter)};
    return true;
}
}

bool collide(const CollisionShape& first, const CollisionShape& second, Manifold& result)
{
    Manifold found;
    if (!detect(first, second, found)) {
        return false;
    }
    result = found;
    return true;
}

bool overlaps(const CollisionShape& first, const CollisionShape& second)
{
    Manifold found;
    return collide(first, second, found);
}

float distance(const CollisionShape& first, const CollisionShape& second)
{
    if (overlaps(first, second)) {
        return 0;
    }
    const bool firstIsCircle = first.kind() == CollisionShape::Kind::Circle;
    const bool secondIsCircle = second.kind() == CollisionShape::Kind::Circle;
    if (firstIsCircle && secondIsCircle) {
        const Circle& circleFirst = first.circle();
        const Circle& circleSecond = second.circle();
        const double offsetX = static_cast<double>(circleSecond.center.x) - circleFirst.center.x;
        const double offsetY = static_cast<double>(circleSecond.center.y) - circleFirst.center.y;
        const double gap = std::hypot(offsetX, offsetY) - circleFirst.radius - circleSecond.radius;
        return static_cast<float>(std::max(0.0, gap));
    }
    if (firstIsCircle) {
        const double gap = pointGap(first.circle().center, boxOf(second)) - first.circle().radius;
        return static_cast<float>(std::max(0.0, gap));
    }
    if (secondIsCircle) {
        const double gap = pointGap(second.circle().center, boxOf(first)) - second.circle().radius;
        return static_cast<float>(std::max(0.0, gap));
    }
    return static_cast<float>(boxesDistance(boxOf(first), boxOf(second)));
}

bool castSegment(const CollisionShape& shape, Vector2 begin, Vector2 end, SegmentHit& result)
{
    if (shape.kind() == CollisionShape::Kind::Circle) {
        return castCircle(shape.circle(), begin, end, result);
    }
    return castBox(boxOf(shape), begin, end, result);
}
}
