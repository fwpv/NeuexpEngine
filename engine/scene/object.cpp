#include "object.h"
#include "graphics/renderer.h"

namespace NeuexpEngine
{
void GameObject::enablePhysics(const PhysicsBody& settings)
{
    physicsBody = settings;
    physicsBody.wake();
}

void GameObject::disablePhysics()
{
    physicsBody = PhysicsBody{};
}

void GameObject::setPose(Vector2 position, float angle)
{
    placement.position = position;
    placement.rotation = angle;
    physicsBody.wake();
}

void GameObject::applyImpulse(Vector2 impulse)
{
    if (physicsBody.type != BodyType::Dynamic) {
        return;
    }
    const float mass = bodyMass(*this);
    if (mass > 0) {
        physicsBody.velocity += impulse / mass;
    }
}

void GameObject::applyImpulseAtPoint(Vector2 impulse, Vector2 worldPoint)
{
    if (physicsBody.type != BodyType::Dynamic) {
        return;
    }
    const float mass = bodyMass(*this);
    if (mass <= 0) {
        return;
    }
    physicsBody.velocity += impulse / mass;
    const float inertia = bodyInertia(*this);
    if (inertia > 0) {
        physicsBody.angularVelocity += cross(worldPoint - placement.position, impulse) / inertia;
    }
}

void GameObject::applyAngularImpulse(float impulse)
{
    if (physicsBody.type != BodyType::Dynamic) {
        return;
    }
    const float inertia = bodyInertia(*this);
    if (inertia > 0) {
        physicsBody.angularVelocity += impulse / inertia;
    }
}

void GameObject::applyForce(Vector2 force)
{
    if (physicsBody.type == BodyType::Dynamic) {
        physicsBody.force += force;
    }
}

void GameObject::applyForceAtPoint(Vector2 force, Vector2 worldPoint)
{
    if (physicsBody.type != BodyType::Dynamic) {
        return;
    }
    physicsBody.force += force;
    physicsBody.torque += cross(worldPoint - placement.position, force);
}

void GameObject::applyTorque(float torque)
{
    if (physicsBody.type == BodyType::Dynamic) {
        physicsBody.torque += torque;
    }
}

void GameObject::update(float)
{
}

void GameObject::draw(Renderer2D& renderer) const
{
    if (!appearance.texture.isValid()) {
        return;
    }
    DrawOptions options;
    options.transform = placement.toAffine();
    renderer.drawSprite(appearance, options);
}
}
