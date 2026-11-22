#pragma once

#include "collision/collider.h"
#include "collision/shape.h"
#include "core/geometry.h"
#include "core/math2d.h"
#include "scene/object.h"
#include "scene/scene.h"

namespace CollisionDemo
{
// Мир демонстрации занимает фиксированную область 800x600 единиц.
constexpr float WorldWidth = 800;
constexpr float WorldHeight = 600;

// Слои демонстрации: формы и плиты. Клавиша L переключает маску плит.
constexpr NeuexpEngine::CollisionLayer ShapeLayer = 1U;
constexpr NeuexpEngine::CollisionLayer SlabLayer = 2U;

// Группы объектов и имя триггерной зоны: по ним работают переключатели демонстрации.
inline constexpr char OrbGroup[] = "orb";
inline constexpr char SlabGroup[] = "slab";
inline constexpr char GearGroup[] = "gear";
inline constexpr char WalkerGroup[] = "walker";
inline constexpr char ZoneGroup[] = "zone";
inline constexpr char ZoneName[] = "zone";

// Движение объекта: орбита вокруг центра, колебание по одной оси или вращение на месте.
enum class Motion
{
    Orbit,
    SwayX,
    SwayY,
    Spin
};

// Объект, который сам двигает своё преобразование. Форма столкновений остаётся
// локальной, а её вид в мире считается из преобразования при каждой проверке.
class Mover : public NeuexpEngine::GameObject
{
public:
    Mover(NeuexpEngine::Vector2 origin, Motion motion, float amplitude, float speed, float phase);
    void update(float deltaTime) override;

private:
    void applyPlacement();

    NeuexpEngine::Vector2 origin;
    Motion motion = Motion::Orbit;
    float amplitude = 0;
    float speed = 0;
    float phase = 0;
    float elapsed = 0;
};

// Центр формы в мировых координатах: отметки контактов и подписи считаются по нему.
NeuexpEngine::Vector2 shapeCenter(const NeuexpEngine::CollisionShape& shape);

// Наполнение сцены: круги, плиты вдоль осей и повёрнутые бруски, а также триггерная зона.
void populate(NeuexpEngine::Scene& scene);
}
