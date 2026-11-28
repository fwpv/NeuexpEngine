#include "world.h"
#include "core/geometry.h"
#include "collision/shape.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace PhysicsDemo
{
namespace
{
using namespace NeuexpEngine;

// Мир перезапускается и живёт долго: таймер добавляет шары, а тела ограничены,
// чтобы демонстрация не росла бесконечно.
constexpr int SpawnIntervalSteps = 90;
constexpr std::size_t MaximumBodies = 34;
constexpr float WindAcceleration = 800;
constexpr float AttractionAcceleration = 1400;
constexpr float ClickImpulseSpeed = 600;
constexpr float PlatformSpeed = 90;
constexpr float PlatformLeft = 720;
constexpr float PlatformRight = 880;
constexpr int CrateImageSize = 32;

const Color StaticFill{0.16F, 0.19F, 0.24F, 1};
const Color StaticOutline{0.35F, 0.4F, 0.5F, 1};
const Color PlatformFill{0.28F, 0.52F, 0.72F, 1};
const Color PlatformOutline{0.55F, 0.8F, 0.95F, 1};
const Color GateFill{0.55F, 0.4F, 1, 0.12F};
const Color GateOutline{0.7F, 0.55F, 1, 0.8F};
const Color GateActiveFill{0.55F, 0.4F, 1, 0.3F};
const Color PitFill{1, 0.35F, 0.3F, 0.1F};
const Color PitOutline{1, 0.45F, 0.35F, 0.7F};
const Color BallFill{0.35F, 0.8F, 0.95F, 1};
const Color BallOutline{0.8F, 0.95F, 1, 1};
const Color ContactColor{1, 0.85F, 0.3F, 1};

Image makeCrateImage()
{
    Image image{CrateImageSize, CrateImageSize,
        std::vector<std::uint8_t>(static_cast<std::size_t>(CrateImageSize) * CrateImageSize * 4, 0)};
    for (int y = 0; y < CrateImageSize; ++y) {
        for (int x = 0; x < CrateImageSize; ++x) {
            const bool border = x == 0 || y == 0 || x == CrateImageSize - 1 || y == CrateImageSize - 1;
            const bool plank = x % 8 == 0 || y % 8 == 0;
            Color color{0.66F, 0.48F, 0.28F, 1};
            if (y < CrateImageSize / 2) {
                color = {0.72F, 0.53F, 0.31F, 1};
            }
            if (plank) {
                color = {0.44F, 0.31F, 0.17F, 1};
            }
            if (border) {
                color = {0.28F, 0.18F, 0.1F, 1};
            }
            const std::size_t offset = (static_cast<std::size_t>(y) * CrateImageSize
                + static_cast<std::size_t>(x)) * 4;
            image.pixels[offset] = static_cast<std::uint8_t>(color.red * 255);
            image.pixels[offset + 1] = static_cast<std::uint8_t>(color.green * 255);
            image.pixels[offset + 2] = static_cast<std::uint8_t>(color.blue * 255);
            image.pixels[offset + 3] = 255;
        }
    }
    return image;
}

void drawShape(Renderer2D& drawing, const CollisionShape& shape, Color fill, Color outline)
{
    DrawOptions filled;
    filled.color = fill;
    DrawOptions border;
    border.color = outline;
    border.lineWidth = 2;
    if (shape.kind() == CollisionShape::Kind::Circle) {
        drawing.drawCircle(shape.circle(), filled);
        drawing.drawCircle(shape.circle(), border, ShapeFill::Outline);
        return;
    }
    if (shape.kind() == CollisionShape::Kind::Rect) {
        drawing.drawRectangle(shape.rect(), filled);
        drawing.drawRectangle(shape.rect(), border, ShapeFill::Outline);
        return;
    }
    drawing.drawPolygon(shape.box().corners(), filled);
    drawing.drawPolygon(shape.box().corners(), border, ShapeFill::Outline);
}
}

bool World::loadAssets(Renderer2D& renderer, ResourceError& error)
{
    const Image image = makeCrateImage();
    return renderer.createTexture(image, crateTexture, error);
}

void World::recreate()
{
    scene.clear();
    simulation.clear();
    stats = {};
    platformId = 0;
    gateId = 0;
    pitId = 0;
    wind = false;
    attracting = false;
    spawnIndex = 0;
    spawnTimer = 0;
    simulation.gravity = {0, 900};

    // Пол с разрывом посередине: тела проваливаются в сенсор-яму и удаляются.
    addStatic({215, 660}, {430, 80});
    addStatic({885, 660}, {430, 80});
    addStatic({15, 350}, {30, 700});
    addStatic({1085, 350}, {30, 700});
    addStatic({300, 500}, {300, 22}, -0.6F);
    addStatic({760, 400}, {260, 22});
    pitId = addSensor({550, 690}, {240, 130});
    gateId = addSensor({550, 260}, {260, 120});
    addPlatform();

    addCrate({100, 60}, {40, 40}, 0.25F, 1.2F);
    addCrate({380, 110}, {40, 40}, -0.3F, 1.2F);
    addCrate({310, 90}, {40, 40}, 0.15F, 1.2F);
    addCrate({700, 180}, {60, 40}, 0.1F, 3);
    addCrate({930, 120}, {40, 40}, 0.5F, 1.2F, 1.2F);
    addCrate({800, 492}, {36, 36}, 0, 1.2F);
    addCrate({1020, 600}, {40, 40}, 0, 1.2F);
    addCrate({1020, 560}, {40, 40}, 0, 1.2F);
    addCrate({1020, 520}, {40, 40}, 0, 1.2F);

    addBall({470, 70}, 14);
    addBall({550, 120}, 16);
    addBall({630, 60}, 15);
    addBall({520, 160}, 18);
    addBall({320, 60}, 14);
    addBall({760, 60}, 16);
}

void World::step(float deltaTime)
{
    updatePlatform();
    applyForces();
    simulation.step(deltaTime);
    simulation.sync(scene);
    handleEvents();
    scene.update(0);
    ++spawnTimer;
    if (spawnTimer >= SpawnIntervalSteps) {
        spawnTimer = 0;
        if (simulation.bodyCount() < MaximumBodies) {
            spawnBall();
        }
    }
}

void World::draw(Renderer2D& drawing) const
{
    const RigidBody* gate = simulation.body(gateId);
    const RigidBody* platform = simulation.body(platformId);
    for (const RigidBody* body : simulation.bodies()) {
        if (body->type == BodyType::Dynamic) {
            // У тел с объектом сцены есть спрайт: их рисует сцена.
            if (body->object != 0) {
                continue;
            }
            const CollisionShape shape = body->worldShape();
            drawShape(drawing, shape, BallFill, BallOutline);
            if (shape.kind() == CollisionShape::Kind::Circle) {
                const Circle& circle = shape.circle();
                drawing.drawLine(circle.center,
                    circle.center + rotated(Vector2{circle.radius, 0}, body->angle), BallOutline);
            }
            continue;
        }
        if (body->sensor) {
            const bool gateSensor = body == gate;
            const Color fill = gateSensor && stats.gateInside > 0 ? GateActiveFill : (gateSensor ? GateFill : PitFill);
            drawShape(drawing, body->worldShape(), fill, gateSensor ? GateOutline : PitOutline);
            continue;
        }
        if (body == platform) {
            drawShape(drawing, body->worldShape(), PlatformFill, PlatformOutline);
            continue;
        }
        drawShape(drawing, body->worldShape(), StaticFill, StaticOutline);
    }
    scene.draw(drawing);
    // Точки контактов текущего шага показывают, где решатель приложил импульсы.
    for (const PhysicsContactEvent& event : simulation.contactEvents()) {
        if (event.contact.sensor || event.phase == ContactPhase::End) {
            continue;
        }
        for (int index = 0; index < event.contact.pointCount; ++index) {
            drawing.drawCircle(Circle{event.contact.points[static_cast<std::size_t>(index)], 3}, ContactColor);
        }
    }
}

void World::toggleWind()
{
    wind = !wind;
}

bool World::impulseAt(Vector2 point)
{
    const std::vector<RigidBody*> bodies = simulation.bodies();
    for (auto body = bodies.rbegin(); body != bodies.rend(); ++body) {
        RigidBody& candidate = **body;
        if (candidate.type != BodyType::Dynamic || !contains(candidate.worldShape(), point)) {
            continue;
        }
        candidate.applyImpulseAtPoint(Vector2{0, -ClickImpulseSpeed * candidate.mass()}, point);
        return true;
    }
    return false;
}

void World::spawnBall()
{
    const float position = spawnIndex % 3 == 0
        ? 150.0F + static_cast<float>((spawnIndex * 37) % 250)
        : 440.0F + static_cast<float>((spawnIndex * 61) % 220);
    const float radius = 13.0F + static_cast<float>(spawnIndex % 4) * 1.5F;
    addBall({position, 30}, radius);
    ++spawnIndex;
}

void World::addStatic(Vector2 center, Size2 size, float angle)
{
    simulation.addBody(RigidBody{Box{{0, 0}, size, 0}, BodyType::Static, center, angle});
}

BodyId World::addSensor(Vector2 center, Size2 size)
{
    RigidBody body{Box{{0, 0}, size, 0}, BodyType::Static, center};
    body.sensor = true;
    return simulation.addBody(body);
}

void World::addPlatform()
{
    RigidBody platform{Box{{0, 0}, {150, 20}, 0}, BodyType::Kinematic, {800, 520}};
    platform.velocity = {PlatformSpeed, 0};
    platform.friction = 0.9F;
    platformId = simulation.addBody(platform);
}

void World::addCrate(Vector2 position, Size2 size, float angle, float density, float angularVelocity)
{
    RigidBody body{Box{{0, 0}, size, 0}, BodyType::Dynamic, position, angle};
    body.density = density;
    body.friction = 0.55F;
    body.restitution = 0.1F;
    body.angularVelocity = angularVelocity;
    auto object = std::make_unique<GameObject>();
    object->transform().position = position;
    object->transform().rotation = angle;
    object->visual() = crateSprite(size);
    body.object = scene.add(std::move(object));
    simulation.addBody(body);
}

void World::addBall(Vector2 position, float radius)
{
    RigidBody body{Circle{{0, 0}, radius}, BodyType::Dynamic, position};
    body.density = 0.9F;
    body.friction = 0.35F;
    body.restitution = 0.72F;
    simulation.addBody(body);
}

Sprite World::crateSprite(Size2 size) const
{
    Sprite sprite{crateTexture};
    sprite.size = size;
    sprite.filter = TextureFilter::Nearest;
    sprite.transform.origin = Vector2{size.width / 2, size.height / 2};
    return sprite;
}

void World::updatePlatform()
{
    RigidBody* platform = simulation.body(platformId);
    if (platform == nullptr) {
        return;
    }
    if (platform->position.x <= PlatformLeft) {
        platform->velocity.x = PlatformSpeed;
    } else if (platform->position.x >= PlatformRight) {
        platform->velocity.x = -PlatformSpeed;
    }
}

void World::applyForces()
{
    if (!wind && !attracting) {
        return;
    }
    for (RigidBody* body : simulation.bodies()) {
        if (body->type != BodyType::Dynamic) {
            continue;
        }
        if (wind) {
            // Сила пропорциональна массе: ускорение от ветра одинаково для всех тел.
            body->applyForce(Vector2{body->mass() * WindAcceleration, 0});
        }
        if (attracting) {
            const Vector2 offset = caret - body->position;
            if (lengthSquared(offset) > 1) {
                body->applyForce(normalized(offset) * (body->mass() * AttractionAcceleration));
            }
        }
    }
}

void World::handleEvents()
{
    stats.contacts = 0;
    for (const PhysicsContactEvent& event : simulation.contactEvents()) {
        const PhysicsContact& contact = event.contact;
        const bool gate = contact.first == gateId || contact.second == gateId;
        const bool pit = contact.first == pitId || contact.second == pitId;
        if (contact.sensor) {
            if (gate) {
                if (event.phase == ContactPhase::Begin) {
                    ++stats.gateBegins;
                    ++stats.gateInside;
                } else if (event.phase == ContactPhase::End) {
                    ++stats.gateEnds;
                    --stats.gateInside;
                }
            }
            // Сенсор-яма удаляет вошедшее тело: удаление отложено и безопасно
            // во время обработки событий шага.
            if (pit && event.phase == ContactPhase::Begin) {
                const BodyId other = contact.first == pitId ? contact.second : contact.first;
                const RigidBody* target = simulation.body(other);
                if (target != nullptr) {
                    if (target->object != 0) {
                        scene.destroy(target->object);
                    }
                    simulation.destroyBody(other);
                    ++stats.pitRemoved;
                }
            }
            continue;
        }
        if (event.phase != ContactPhase::End) {
            ++stats.contacts;
        }
        if (event.phase == ContactPhase::Begin) {
            ++stats.impacts;
            stats.lastImpactImpulse = contact.normalImpulse;
        }
    }
}
}
