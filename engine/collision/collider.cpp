#include "collider.h"

namespace NeuexpEngine
{
Collider::Collider(const CollisionShape& value)
    : shape(value)
    , enabled(true)
{
}

CollisionShape Collider::worldShape(const Transform2& transform) const
{
    return transformedShape(shape, transform);
}

bool canCollide(const Collider& first, const Collider& second)
{
    if (!first.enabled || !second.enabled) {
        return false;
    }
    return layersMatch(first.layer, first.mask, second.layer, second.mask);
}

bool layersMatch(CollisionLayer firstLayer, CollisionLayer firstMask, CollisionLayer secondLayer,
    CollisionLayer secondMask)
{
    return (firstLayer & secondMask) != 0 && (secondLayer & firstMask) != 0;
}
}
