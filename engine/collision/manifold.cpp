#include "manifold.h"
#include "shape.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace NeuexpEngine
{
namespace
{
// Параллельные грани после поворотов дают малую положительную щель; такие точки
// остаются контактом покоя, чтобы стопки не проседали на каждом шаге.
constexpr float ContactTolerance = 0.05F;

struct BoxGeometry
{
    Quad corners;
    Vector2 axisX;
    Vector2 axisY;
    Size2 half;
};

BoxGeometry geometryOf(const Box& box)
{
    BoxGeometry geometry;
    geometry.corners = box.corners();
    geometry.axisX = rotated(Vector2{1, 0}, box.rotation);
    geometry.axisY = rotated(Vector2{0, 1}, box.rotation);
    geometry.half = Size2{box.size.width / 2, box.size.height / 2};
    return geometry;
}

// Проекция полуразмера прямоугольника на мировую ось; в double ради точности проверки.
double radiusAlong(const BoxGeometry& geometry, Vector2 axis)
{
    const double projectionX = std::abs(static_cast<double>(dot(axis, geometry.axisX)));
    const double projectionY = std::abs(static_cast<double>(dot(axis, geometry.axisY)));
    return static_cast<double>(geometry.half.width) * projectionX
        + static_cast<double>(geometry.half.height) * projectionY;
}

// Наружная нормаль грани corners[first] -> corners[second]; обход вершин даёт её
// поворотом ребра на -90 градусов.
Vector2 faceNormal(Vector2 begin, Vector2 end)
{
    const Vector2 edge = end - begin;
    return normalized(Vector2{edge.y, -edge.x});
}

bool circles(const Circle& first, const Circle& second, ContactManifold& result)
{
    // Проверка пересечения повторяет collision/query: решение в double, результат — float.
    const double offsetX = static_cast<double>(second.center.x) - first.center.x;
    const double offsetY = static_cast<double>(second.center.y) - first.center.y;
    const double radii = static_cast<double>(first.radius) + second.radius;
    const double offsetSquared = offsetX * offsetX + offsetY * offsetY;
    if (offsetSquared > radii * radii) {
        return false;
    }
    if (offsetSquared == 0) {
        result.normal = {1, 0};
        result.depth = static_cast<float>(radii);
    } else {
        const double offsetLength = std::sqrt(offsetSquared);
        result.normal = {static_cast<float>(offsetX / offsetLength),
            static_cast<float>(offsetY / offsetLength)};
        result.depth = static_cast<float>(radii - offsetLength);
    }
    const Vector2 surfaceFirst = first.center + result.normal * first.radius;
    const Vector2 surfaceSecond = second.center - result.normal * second.radius;
    result.points[0] = {(surfaceFirst + surfaceSecond) * 0.5F, result.depth, 0};
    result.count = 1;
    return true;
}

bool circleBox(const Circle& circle, const Box& box, ContactManifold& result)
{
    const Vector2 localCenter = rotated(circle.center - box.center, -box.rotation);
    const Size2 half{box.size.width / 2, box.size.height / 2};
    const Vector2 closest{std::clamp(localCenter.x, -half.width, half.width),
        std::clamp(localCenter.y, -half.height, half.height)};
    const double offsetX = static_cast<double>(closest.x) - localCenter.x;
    const double offsetY = static_cast<double>(closest.y) - localCenter.y;
    const double offsetSquared = offsetX * offsetX + offsetY * offsetY;
    const double radius = circle.radius;
    if (offsetSquared > radius * radius) {
        return false;
    }
    result.count = 1;
    if (offsetSquared > 0) {
        const double offsetLength = std::sqrt(offsetSquared);
        result.normal = rotated(Vector2{static_cast<float>(offsetX / offsetLength),
            static_cast<float>(offsetY / offsetLength)}, box.rotation);
        result.depth = static_cast<float>(radius - offsetLength);
        // Особенность — грань или угол прямоугольника, к которым ближе центр круга.
        const bool onLeft = closest.x == -half.width;
        const bool onRight = closest.x == half.width;
        const bool onTop = closest.y == -half.height;
        const bool onBottom = closest.y == half.height;
        std::uint16_t feature = 0;
        if (onLeft && onTop) {
            feature = 4;
        } else if (onRight && onTop) {
            feature = 5;
        } else if (onRight && onBottom) {
            feature = 6;
        } else if (onLeft && onBottom) {
            feature = 7;
        } else if (onLeft) {
            feature = 0;
        } else if (onRight) {
            feature = 1;
        } else if (onTop) {
            feature = 2;
        } else if (onBottom) {
            feature = 3;
        }
        result.points[0] = {box.center + rotated(closest, box.rotation), result.depth, feature};
        return true;
    }
    // Центр круга внутри: выталкивает ближайшая грань.
    const BoxFaceHit face = nearestBoxFace(localCenter, half);
    result.normal = rotated(-face.normal, box.rotation);
    result.depth = static_cast<float>(radius + face.distance);
    const Vector2 facePoint = localCenter + face.normal * static_cast<float>(face.distance);
    result.points[0] = {box.center + rotated(facePoint, box.rotation), result.depth, face.index};
    return true;
}

// Отсечение отрезка плоскостью: keepGreater оставляет точки normal * x >= offset.
int clipSegment(const std::array<Vector2, 2>& points, Vector2 normal, float offset, bool keepGreater,
    std::array<Vector2, 2>& result)
{
    const float first = dot(normal, points[0]) - offset;
    const float second = dot(normal, points[1]) - offset;
    const bool firstInside = keepGreater ? first >= 0 : first <= 0;
    const bool secondInside = keepGreater ? second >= 0 : second <= 0;
    int count = 0;
    if (firstInside) {
        result[static_cast<std::size_t>(count)] = points[0];
        ++count;
    }
    if (firstInside != secondInside) {
        const float fraction = first / (first - second);
        result[static_cast<std::size_t>(count)] = points[0] + (points[1] - points[0]) * fraction;
        ++count;
    }
    if (secondInside && count < 2) {
        result[static_cast<std::size_t>(count)] = points[1];
        ++count;
    }
    return count;
}

bool boxes(const Box& first, const Box& second, ContactManifold& result)
{
    const BoxGeometry a = geometryOf(first);
    const BoxGeometry b = geometryOf(second);
    const std::array<Vector2, 4> axes = {a.axisX, a.axisY, b.axisX, b.axisY};
    // Разделяющие оси — по две на прямоугольник. Глубина по оси — наименьший сдвиг
    // второй формы: проверка повторяет collision/query, решение в double.
    double bestDepth = 0;
    std::size_t bestAxis = 0;
    Vector2 bestNormal{1, 0};
    for (std::size_t index = 0; index < axes.size(); ++index) {
        const Vector2 axis = axes[index];
        const double centerFirst = static_cast<double>(dot(axis, first.center));
        const double centerSecond = static_cast<double>(dot(axis, second.center));
        const double radiusFirst = radiusAlong(a, axis);
        const double radiusSecond = radiusAlong(b, axis);
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
            bestAxis = index;
            bestNormal = normal;
        }
    }
    result.normal = bestNormal;
    result.depth = static_cast<float>(bestDepth);

    // Опорная грань принадлежит телу, ось которого дала наименьшее перекрытие.
    const bool firstIsReference = bestAxis < 2;
    const BoxGeometry& reference = firstIsReference ? a : b;
    const BoxGeometry& incident = firstIsReference ? b : a;
    const Vector2 referenceDirection = firstIsReference ? result.normal : -result.normal;

    std::size_t referenceFace = 0;
    float bestAlignment = -2;
    for (std::size_t index = 0; index < 4; ++index) {
        const Vector2 normal = faceNormal(reference.corners[index], reference.corners[(index + 1) % 4]);
        const float alignment = dot(normal, referenceDirection);
        if (alignment > bestAlignment) {
            bestAlignment = alignment;
            referenceFace = index;
        }
    }
    const Vector2 referenceBegin = reference.corners[referenceFace];
    const Vector2 referenceEnd = reference.corners[(referenceFace + 1) % 4];
    const Vector2 referenceNormal = faceNormal(referenceBegin, referenceEnd);

    std::size_t incidentFace = 0;
    float bestOpposite = 2;
    for (std::size_t index = 0; index < 4; ++index) {
        const Vector2 normal = faceNormal(incident.corners[index], incident.corners[(index + 1) % 4]);
        const float alignment = dot(normal, referenceDirection);
        if (alignment < bestOpposite) {
            bestOpposite = alignment;
            incidentFace = index;
        }
    }
    const std::array<Vector2, 2> incidentFacePoints = {
        incident.corners[incidentFace], incident.corners[(incidentFace + 1) % 4]};

    // Вершины грани второго тела отсекаются боковыми плоскостями опорной грани:
    // начало отрезка опорной грани и его конец.
    const Vector2 tangent = normalized(referenceEnd - referenceBegin);
    std::array<Vector2, 2> clipped;
    const int clippedCount = clipSegment(
        incidentFacePoints, tangent, dot(tangent, referenceBegin), true, clipped);
    std::array<Vector2, 2> facePoints = {};
    int faceCount = 0;
    const float endProjection = dot(tangent, referenceEnd);
    for (int index = 0; index < clippedCount; ++index) {
        const std::size_t position = static_cast<std::size_t>(index);
        if (dot(tangent, clipped[position]) <= endProjection + ContactTolerance) {
            facePoints[static_cast<std::size_t>(faceCount)] = clipped[position];
            ++faceCount;
        }
    }

    // Особенность точки: номер опорной и падающей граней и вид точки (вершина падающей
    // грани или точка отсечения). Идентификатор не меняется между шагами, пока пара
    // граней та же.
    const std::uint16_t base = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(referenceFace) << 12)
        | (static_cast<std::uint16_t>(incidentFace) << 8));
    result.count = 0;
    for (int index = 0; index < faceCount; ++index) {
        const std::size_t position = static_cast<std::size_t>(index);
        const float separation = dot(referenceNormal, facePoints[position] - referenceBegin);
        if (separation > ContactTolerance) {
            continue;
        }
        std::uint16_t feature = 2;
        if (facePoints[position] == incidentFacePoints[0]) {
            feature = 0;
        } else if (facePoints[position] == incidentFacePoints[1]) {
            feature = 1;
        }
        result.points[static_cast<std::size_t>(result.count)] = {
            facePoints[position], std::max(0.0F, -separation),
            static_cast<std::uint16_t>(base | feature)};
        ++result.count;
    }
    if (result.count == 0) {
        // Глубочайшая вершина второго тела: контакт углом или глубокое перекрытие,
        // когда отсечённые точки не попали на опорную грань.
        float bestSeparation = std::numeric_limits<float>::max();
        Vector2 bestPoint = incidentFacePoints[0];
        std::uint16_t bestCorner = 0;
        for (std::size_t cornerIndex = 0; cornerIndex < incident.corners.size(); ++cornerIndex) {
            const Vector2 corner = incident.corners[cornerIndex];
            const float separation = dot(referenceNormal, corner - referenceBegin);
            if (separation < bestSeparation) {
                bestSeparation = separation;
                bestPoint = corner;
                bestCorner = static_cast<std::uint16_t>(cornerIndex);
            }
        }
        result.points[0] = {bestPoint, std::max(0.0F, -bestSeparation),
            static_cast<std::uint16_t>(base | 3U | (bestCorner << 2))};
        result.count = 1;
    }
    return true;
}
}

bool detectContact(const CollisionShape& first, const CollisionShape& second, ContactManifold& result)
{
    const bool firstIsCircle = first.kind() == CollisionShape::Kind::Circle;
    const bool secondIsCircle = second.kind() == CollisionShape::Kind::Circle;
    if (firstIsCircle && secondIsCircle) {
        return circles(first.circle(), second.circle(), result);
    }
    if (firstIsCircle) {
        return circleBox(first.circle(), boxOf(second), result);
    }
    if (secondIsCircle) {
        ContactManifold reversed;
        if (!circleBox(second.circle(), boxOf(first), reversed)) {
            return false;
        }
        result = reversed;
        result.normal = -reversed.normal;
        return true;
    }
    return boxes(boxOf(first), boxOf(second), result);
}
}
