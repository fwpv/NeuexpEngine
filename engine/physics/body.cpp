#include "body.h"
#include "scene/object.h"

namespace NeuexpEngine
{
namespace
{
float areaOf(const CollisionShape& shape)
{
    if (shape.kind() == CollisionShape::Kind::Circle) {
        const float radius = shape.circle().radius;
        return Pi * radius * radius;
    }
    const Size2 size = shape.kind() == CollisionShape::Kind::Rect ? shape.rect().size : shape.box().size;
    return size.width * size.height;
}
}

BodyMass bodyMassData(const CollisionShape& shape, Vector2 pivot, float density)
{
    const float mass = density * areaOf(shape);
    float inertia = 0;
    if (shape.kind() == CollisionShape::Kind::Circle) {
        const float radius = shape.circle().radius;
        const float offset = lengthSquared(shape.circle().center - pivot);
        inertia = mass * (radius * radius / 2 + offset);
    } else {
        const Size2 size = shape.kind() == CollisionShape::Kind::Rect ? shape.rect().size : shape.box().size;
        const Vector2 center = shape.kind() == CollisionShape::Kind::Rect ? shape.rect().center() : shape.box().center;
        const float offset = lengthSquared(center - pivot);
        inertia = mass * ((size.width * size.width + size.height * size.height) / 12 + offset);
    }
    return {mass, inertia};
}

float bodyMass(const GameObject& object)
{
    const CollisionShape shape = object.collider().worldShape(object.transform());
    return bodyMassData(shape, object.transform().position, object.body().density).mass;
}

float bodyInertia(const GameObject& object)
{
    const CollisionShape shape = object.collider().worldShape(object.transform());
    return bodyMassData(shape, object.transform().position, object.body().density).inertia;
}
}
