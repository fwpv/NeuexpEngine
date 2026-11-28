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
