#include "world.h"

#include <cmath>
#include <memory>
#include <utility>

namespace CollisionDemo
{
namespace
{
void addMover(NeuexpEngine::Scene& scene, const char* name, const char* group,
    NeuexpEngine::CollisionLayer layer, const NeuexpEngine::CollisionShape& shape,
    NeuexpEngine::Vector2 origin, Motion motion, float amplitude, float speed, float phase)
{
    auto object = std::make_unique<Mover>(origin, motion, amplitude, speed, phase);
    object->setName(name);
    object->setGroup(group);
    object->collider() = NeuexpEngine::Collider{shape};
    object->collider().layer = layer;
    scene.add(std::move(object));
}

void addZone(NeuexpEngine::Scene& scene, NeuexpEngine::Vector2 center, float radius)
{
    auto zone = std::make_unique<NeuexpEngine::GameObject>();
    zone->setName(ZoneName);
    zone->setGroup(ZoneGroup);
    zone->transform().position = center;
    zone->collider() = NeuexpEngine::Collider{NeuexpEngine::Circle{NeuexpEngine::Vector2{}, radius}};
    zone->collider().layer = ShapeLayer;
    zone->collider().trigger = true;
    scene.add(std::move(zone));
}
}

Mover::Mover(NeuexpEngine::Vector2 origin, Motion motion, float amplitude, float speed, float phase)
    : origin(origin)
    , motion(motion)
    , amplitude(amplitude)
    , speed(speed)
    , phase(phase)
{
    // Начальное положение задаётся тем же правилом, что и движение: первый кадр
    // уже показывает объект на своём месте.
    applyPlacement();
}

void Mover::update(float deltaTime)
{
    elapsed += deltaTime;
    applyPlacement();
}

void Mover::applyPlacement()
{
    const float angle = phase + elapsed * speed;
    if (motion == Motion::Orbit) {
        transform().position = origin
            + NeuexpEngine::Vector2{std::cos(angle) * amplitude, std::sin(angle) * amplitude};
        return;
    }
    if (motion == Motion::SwayX) {
        transform().position = {origin.x + std::sin(angle) * amplitude, origin.y};
        return;
    }
    if (motion == Motion::SwayY) {
        transform().position = {origin.x, origin.y + std::sin(angle) * amplitude};
        return;
    }
    // Поворот на месте: положение остаётся в центре движения, а вид формы в мире
    // меняется из-за поворота преобразования.
    transform().position = origin;
    transform().rotation = angle;
}

NeuexpEngine::Vector2 shapeCenter(const NeuexpEngine::CollisionShape& shape)
{
    if (shape.kind() == NeuexpEngine::CollisionShape::Kind::Circle) {
        return shape.circle().center;
    }
    if (shape.kind() == NeuexpEngine::CollisionShape::Kind::Rect) {
        return shape.rect().center();
    }
    return shape.box().center;
}

void populate(NeuexpEngine::Scene& scene)
{
    // Круг, прямоугольник вдоль осей и повёрнутый прямоугольник движутся так,
    // чтобы пересекаться друг с другом.
    addMover(scene, "orb-a", OrbGroup, ShapeLayer, NeuexpEngine::Circle{NeuexpEngine::Vector2{}, 24},
        NeuexpEngine::Vector2{400, 300}, Motion::Orbit, 150, 0.9F, 0);
    addMover(scene, "orb-b", OrbGroup, ShapeLayer, NeuexpEngine::Circle{NeuexpEngine::Vector2{}, 24},
        NeuexpEngine::Vector2{400, 300}, Motion::Orbit, 165, -0.6F, 1.0F);
    // Третий круг ходит вокруг брусков: так встречаются круг и повёрнутый прямоугольник.
    addMover(scene, "orb-c", OrbGroup, ShapeLayer, NeuexpEngine::Circle{NeuexpEngine::Vector2{}, 20},
        NeuexpEngine::Vector2{400, 120}, Motion::Orbit, 70, 1.4F, 2.0F);
    addMover(scene, "slab-a", SlabGroup, SlabLayer, NeuexpEngine::Rect{-55, -16, 110, 32},
        NeuexpEngine::Vector2{400, 300}, Motion::SwayX, 220, 0.7F, 0);
    addMover(scene, "slab-b", SlabGroup, SlabLayer, NeuexpEngine::Rect{-55, -16, 110, 32},
        NeuexpEngine::Vector2{400, 300}, Motion::SwayY, 200, 0.5F, 0.6F);
    addMover(scene, "gear-a", GearGroup, ShapeLayer, NeuexpEngine::Box{NeuexpEngine::Vector2{},
        NeuexpEngine::Size2{150, 28}, 0}, NeuexpEngine::Vector2{400, 120}, Motion::Spin, 0, 0.9F, 0);
    addMover(scene, "gear-b", GearGroup, ShapeLayer, NeuexpEngine::Box{NeuexpEngine::Vector2{},
        NeuexpEngine::Size2{150, 28}, 0}, NeuexpEngine::Vector2{400, 120}, Motion::Spin, 0, -1.3F, 0.5F);
    addMover(scene, "walker", WalkerGroup, ShapeLayer, NeuexpEngine::Circle{NeuexpEngine::Vector2{}, 20},
        NeuexpEngine::Vector2{660, 470}, Motion::SwayX, 130, 0.6F, 0);
    addZone(scene, NeuexpEngine::Vector2{660, 470}, 85);
}
}
