#include "body.h"
#include "core/transform.h"

#include <cassert>
#include <cmath>

namespace NeuexpEngine
{
namespace
{
Vector2 centerOf(const CollisionShape& shape)
{
    if (shape.kind() == CollisionShape::Kind::Circle) {
        return shape.circle().center;
    }
    if (shape.kind() == CollisionShape::Kind::Rect) {
        return shape.rect().center();
    }
    return shape.box().center;
}

Size2 sizeOf(const CollisionShape& shape)
{
    if (shape.kind() == CollisionShape::Kind::Rect) {
        return shape.rect().size;
    }
    return shape.box().size;
}
}

RigidBody::RigidBody(const CollisionShape& shape, BodyType type, Vector2 position, float angle)
    : shape(shape)
    , type(type)
    , position(position)
    , angle(angle)
{
}

void RigidBody::setPose(Vector2 newPosition, float newAngle)
{
    assert(std::isfinite(newPosition.x) && std::isfinite(newPosition.y)
        && std::isfinite(newAngle));
    position = newPosition;
    angle = newAngle;
    sleeping = false;
    sleepTime = 0;
}

CollisionShape RigidBody::worldShape() const
{
    // Масштаб и origin объекта сцены к телу не применяются: размер задаёт форма.
    return transformedShape(shape, Transform2{position, angle, Vector2{1, 1}, Vector2{}});
}

Rect RigidBody::worldBounds() const
{
    return bounds(worldShape());
}

float RigidBody::area() const
{
    if (shape.kind() == CollisionShape::Kind::Circle) {
        const float radius = shape.circle().radius;
        return Pi * radius * radius;
    }
    const Size2 size = sizeOf(shape);
    return size.width * size.height;
}

float RigidBody::mass() const
{
    return density * area();
}

float RigidBody::inertia() const
{
    const float bodyMass = mass();
    const float centerDistance = lengthSquared(centerOf(shape));
    if (shape.kind() == CollisionShape::Kind::Circle) {
        const float radius = shape.circle().radius;
        return bodyMass * (radius * radius / 2 + centerDistance);
    }
    const Size2 size = sizeOf(shape);
    return bodyMass * ((size.width * size.width + size.height * size.height) / 12 + centerDistance);
}

float RigidBody::inverseMass() const
{
    const float bodyMass = mass();
    if (type != BodyType::Dynamic || bodyMass <= 0) {
        return 0;
    }
    return 1 / bodyMass;
}

float RigidBody::inverseInertia() const
{
    const float bodyInertia = inertia();
    if (type != BodyType::Dynamic || bodyInertia <= 0) {
        return 0;
    }
    return 1 / bodyInertia;
}

void RigidBody::applyImpulse(Vector2 impulse)
{
    const float inverse = inverseMass();
    if (inverse == 0) {
        return;
    }
    velocity += impulse * inverse;
}

void RigidBody::applyImpulseAtPoint(Vector2 impulse, Vector2 worldPoint)
{
    const float inverse = inverseMass();
    if (inverse == 0) {
        return;
    }
    velocity += impulse * inverse;
    angularVelocity += cross(worldPoint - position, impulse) * inverseInertia();
}

void RigidBody::applyAngularImpulse(float impulse)
{
    const float inverse = inverseInertia();
    if (inverse == 0) {
        return;
    }
    angularVelocity += impulse * inverse;
}

void RigidBody::applyForce(Vector2 value)
{
    if (type != BodyType::Dynamic) {
        return;
    }
    force += value;
}

void RigidBody::applyForceAtPoint(Vector2 value, Vector2 worldPoint)
{
    if (type != BodyType::Dynamic) {
        return;
    }
    force += value;
    torque += cross(worldPoint - position, value);
}

void RigidBody::applyTorque(float value)
{
    if (type != BodyType::Dynamic) {
        return;
    }
    torque += value;
}
}
