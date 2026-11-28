#include "body.h"
#include "scene/object.h"

#include <cassert>
#include <cmath>

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

// Обратные масса и инерция; у статического и кинематического тела они нулевые.
float inverseMassOf(const GameObject& object)
{
    if (object.body().type != BodyType::Dynamic) {
        return 0;
    }
    const float mass = bodyMass(object);
    return mass > 0 ? 1 / mass : 0;
}

float inverseInertiaOf(const GameObject& object)
{
    if (object.body().type != BodyType::Dynamic) {
        return 0;
    }
    const float inertia = bodyInertia(object);
    return inertia > 0 ? 1 / inertia : 0;
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
    assert(object.physicsEnabled());
    const CollisionShape shape = object.collider().worldShape(object.transform());
    return bodyMassData(shape, object.transform().position, object.body().density).mass;
}

float bodyInertia(const GameObject& object)
{
    assert(object.physicsEnabled());
    const CollisionShape shape = object.collider().worldShape(object.transform());
    return bodyMassData(shape, object.transform().position, object.body().density).inertia;
}

void applyImpulse(GameObject& object, Vector2 impulse)
{
    const float inverse = inverseMassOf(object);
    if (inverse == 0) {
        return;
    }
    object.body().velocity += impulse * inverse;
}

void applyImpulseAtPoint(GameObject& object, Vector2 impulse, Vector2 worldPoint)
{
    const float inverse = inverseMassOf(object);
    if (inverse == 0) {
        return;
    }
    object.body().velocity += impulse * inverse;
    object.body().angularVelocity += cross(worldPoint - object.transform().position, impulse)
        * inverseInertiaOf(object);
}

void applyAngularImpulse(GameObject& object, float impulse)
{
    const float inverse = inverseInertiaOf(object);
    if (inverse == 0) {
        return;
    }
    object.body().angularVelocity += impulse * inverse;
}

void applyForce(GameObject& object, Vector2 force)
{
    if (object.body().type != BodyType::Dynamic) {
        return;
    }
    object.body().force += force;
}

void applyForceAtPoint(GameObject& object, Vector2 force, Vector2 worldPoint)
{
    if (object.body().type != BodyType::Dynamic) {
        return;
    }
    object.body().force += force;
    object.body().torque += cross(worldPoint - object.transform().position, force);
}

void applyTorque(GameObject& object, float torque)
{
    if (object.body().type != BodyType::Dynamic) {
        return;
    }
    object.body().torque += torque;
}
}
