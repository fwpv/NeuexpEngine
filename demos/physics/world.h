#pragma once

#include "core/image.h"
#include "core/math2d.h"
#include "core/resources.h"
#include "core/texture.h"
#include "graphics/renderer.h"
#include "graphics/sprite.h"
#include "physics/body.h"
#include "physics/world.h"
#include "scene/object.h"
#include "scene/scene.h"

#include <cstddef>

namespace PhysicsDemo
{
constexpr float WorldWidth = 1100;
constexpr float WorldHeight = 700;

// Счётчики событий мира для подписей.
struct Statistics
{
    int gateBegins = 0;
    int gateEnds = 0;
    int gateInside = 0;
    int pitRemoved = 0;
    int impacts = 0;
    float lastImpactImpulse = 0;
    int contacts = 0;
};

// Демонстрационный мир: неподвижные конструкции, падающие тела, движущаяся площадка
// и два сенсора. Ящики рисуются спрайтами из трансформаций объектов сцены, остальные
// тела — фигурами по состоянию физики.
class World
{
public:
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Текстура ящика создаётся один раз до первого recreate().
    bool loadAssets(NeuexpEngine::Renderer2D& renderer, NeuexpEngine::ResourceError& error);
    // Строит мир заново: сцена, тела и счётчики возвращаются в исходное состояние.
    void recreate();

    // Один шаг симуляции, события шага и перевод состояния в трансформации объектов.
    void step(float deltaTime);
    void draw(NeuexpEngine::Renderer2D& drawing) const;

    void toggleWind();
    bool isWindActive() const { return wind; }
    void setAttract(bool active) { attracting = active; }
    void setCaret(NeuexpEngine::Vector2 point) { caret = point; }
    // Импульс вверх в точке; false — под точкой нет динамического тела.
    bool impulseAt(NeuexpEngine::Vector2 point);
    // Новый шар над миром; вызывается по клавише и таймером.
    void spawnBall();

    const Statistics& statistics() const { return stats; }
    const NeuexpEngine::PhysicsWorld& physics() const { return simulation; }
    std::size_t bodyCount() const { return simulation.bodyCount(); }

private:
    void addStatic(NeuexpEngine::Vector2 center, NeuexpEngine::Size2 size, float angle = 0);
    NeuexpEngine::BodyId addSensor(NeuexpEngine::Vector2 center, NeuexpEngine::Size2 size);
    void addPlatform();
    void addCrate(NeuexpEngine::Vector2 position, NeuexpEngine::Size2 size, float angle, float density,
        float angularVelocity = 0);
    void addBall(NeuexpEngine::Vector2 position, float radius);
    NeuexpEngine::Sprite crateSprite(NeuexpEngine::Size2 size) const;
    void updatePlatform();
    void applyForces();
    void handleEvents();

    NeuexpEngine::Scene scene;
    NeuexpEngine::PhysicsWorld simulation;
    NeuexpEngine::Texture crateTexture;
    Statistics stats;
    NeuexpEngine::BodyId platformId = 0;
    NeuexpEngine::BodyId gateId = 0;
    NeuexpEngine::BodyId pitId = 0;
    NeuexpEngine::Vector2 caret;
    bool wind = false;
    bool attracting = false;
    int spawnIndex = 0;
    int spawnTimer = 0;
};
}
