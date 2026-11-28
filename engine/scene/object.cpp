#include "object.h"
#include "graphics/renderer.h"

#include <cassert>

namespace NeuexpEngine
{
void GameObject::enablePhysics(const PhysicsBody& settings)
{
    physicsBody = settings;
    physicsBody.sleeping = false;
    physicsBody.sleepTime = 0;
    bodyEnabled = true;
}

void GameObject::disablePhysics()
{
    bodyEnabled = false;
}

const PhysicsBody& GameObject::body() const
{
    assert(bodyEnabled);
    return physicsBody;
}

PhysicsBody& GameObject::body()
{
    assert(bodyEnabled);
    return physicsBody;
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
