#include "entities.h"
#include "core/geometry.h"
#include "core/math2d.h"
#include "graphics/renderer.h"
#include "core/transform.h"

#include <cmath>
#include <optional>
#include <vector>

namespace Game
{
namespace
{
constexpr Vector2 StationPosition = {418.0f, 292.0f};
}

Bolt::Bolt(Vector2 start, Vector2 speed, float lifetime)
    : location(start)
    , velocity(speed)
    , secondsLeft(lifetime)
{
}

void Bolt::update(float deltaTime)
{
    location += velocity * deltaTime;
    secondsLeft -= deltaTime;
}

bool Bolt::isExpired(const Rect& bounds) const
{
    return secondsLeft <= 0 || !NeuexpEngine::contains(bounds, location);
}

Vector2 Bolt::position() const
{
    return location;
}

void Bolt::draw(NeuexpEngine::Renderer2D& renderer) const
{
    DrawOptions options;
    options.lineWidth = 3.0f;
    options.color = Color{0.2f, 0.9f, 1.0f, 0.3f};
    renderer.drawCircle(Circle{location, 8.0f}, options, ShapeFill::Outline, 12);
    options.color = Color{0.72f, 1.0f, 1.0f, 1.0f};
    renderer.drawLine(location - velocity * 0.025f, location, options);
}

void Shuttle::update(const InputState& input, float deltaTime)
{
    Vector2 movement = {0.0f, 0.0f};
    if (input.up) {
        movement.y -= 1.0f;
    }
    if (input.down) {
        movement.y += 1.0f;
    }
    if (input.left) {
        movement.x -= 1.0f;
    }
    if (input.right) {
        movement.x += 1.0f;
    }

    thrusting = movement.x != 0.0f || movement.y != 0.0f;
    if (thrusting) {
        constexpr float MovementSpeed = 205.0f; // Мировые единицы в секунду.
        transform.position += NeuexpEngine::normalized(movement) * MovementSpeed * deltaTime;
        transform.rotation = std::atan2(movement.y, movement.x) + (Pi / 2);
    }

    constexpr Rect MovementBounds = {38, 76, 724, 429};
    transform.position = NeuexpEngine::closestPoint(MovementBounds, transform.position);
}

std::optional<Bolt> Shuttle::fire(bool requested, float time)
{
    if (!requested || time < nextShotTime) {
        return std::nullopt;
    }
    const Affine2 shot = transform.toAffine();
    const Vector2 direction = NeuexpEngine::normalized(shot.transformVector(Vector2{0, -1}));
    constexpr float ShotIntervalSeconds = 0.24f;
    constexpr float BoltSpeed = 470.0f; // Мировые единицы в секунду.
    constexpr float BoltLifetimeSeconds = 2.0f;
    nextShotTime = time + ShotIntervalSeconds;
    return Bolt(shot.transformPoint(Vector2{0, -20.0f}), direction * BoltSpeed, BoltLifetimeSeconds);
}

Vector2 Shuttle::position() const
{
    return transform.position;
}

void Shuttle::teleport(Vector2 destination)
{
    transform.position = destination;
}

void Shuttle::draw(NeuexpEngine::Renderer2D& renderer, float time) const
{
    DrawOptions options;
    options.transform = transform.toAffine();

    if (thrusting) {
        const float flame = 13.0f + 5.0f * std::sin(time * 25.0f);
        options.color = Color{1.0f, 0.3f, 0.05f, 1.0f};
        renderer.drawPolygon(NeuexpEngine::Polygon{Vector2{-7.0f, 13.0f}, Vector2{0, 13.0f + flame}, Vector2{7.0f, 13.0f}}, options);
    }

    options.color = Color{0.14f, 0.56f, 0.78f, 1.0f};
    renderer.drawPolygon(NeuexpEngine::Polygon{
        Vector2{0.0f, -20.0f}, Vector2{7.0f, -5.0f}, Vector2{17.0f, 9.0f}, Vector2{8.0f, 7.0f},
        Vector2{0.0f, 13.0f}, Vector2{-8.0f, 7.0f}, Vector2{-17.0f, 9.0f}, Vector2{-7.0f, -5.0f}
    }, options);

    options.color = Color{0.05f, 0.15f, 0.26f, 1.0f};
    renderer.drawPolygon(NeuexpEngine::Quad{Vector2{-3.0f, 1.0f}, Vector2{3.0f, 1.0f},
        Vector2{5.0f, 7.0f}, Vector2{-5.0f, 7.0f}}, options);
    options.color = Color{0.3f, 0.9f, 1.0f, 0.75f};
    renderer.drawCircle(Circle{Vector2{}, 22.0f}, options, ShapeFill::Outline);
}

Drone::Drone(float startPhase)
    : phase(startPhase)
{
}

Vector2 Drone::position(float time) const
{
    const float angle = time * 0.42f + phase;
    const Transform2 orbit = {Vector2{655.0f, 340.0f}, 0, Vector2{150.0f, 47.0f}, Vector2{}};
    return orbit.toAffine().transformPoint(NeuexpEngine::rotated(Vector2{1, 0}, angle));
}

void Drone::update(float time)
{
    if (!active && time >= respawnTime) {
        active = true;
    }
}

bool Drone::tryHit(Vector2 point, float time)
{
    if (!active || !NeuexpEngine::contains(Circle{position(time), 17.0f}, point)) {
        return false;
    }
    active = false;
    constexpr float RespawnDelaySeconds = 4.0f;
    respawnTime = time + RespawnDelaySeconds;
    return true;
}

void Drone::draw(NeuexpEngine::Renderer2D& renderer, float time) const
{
    if (!active) {
        return;
    }

    const Vector2 center = position(time);
    renderer.drawCircle(Circle{center, 19.0f}, Color{1.0f, 0.12f, 0.18f, 0.2f}, ShapeFill::Filled, 32);
    renderer.drawCircle(Circle{center, 11.0f}, Color{0.48f, 0.08f, 0.18f, 1.0f}, ShapeFill::Filled, 8);
    renderer.drawCircle(Circle{center, 13.0f}, Color{1.0f, 0.25f, 0.3f, 1.0f}, ShapeFill::Outline, 8);
    renderer.drawCircle(Circle{center, 3.0f}, Color{1.0f, 0.82f, 0.75f, 1.0f}, ShapeFill::Filled, 8);
}

Portal::Portal(Vector2 entrance, Vector2 exit)
    : position(entrance)
    , destination(exit)
{
}

bool Portal::tryTeleport(Shuttle& shuttle) const
{
    if (!NeuexpEngine::contains(Circle{position, 28.0f}, shuttle.position())) {
        return false;
    }
    shuttle.teleport(destination);
    return true;
}

void Portal::draw(NeuexpEngine::Renderer2D& renderer, float time, bool open) const
{
    const float brightness = open ? 1.0f : 0.3f;
    const float pulse = 1.0f + 0.12f * std::sin(time * 3.0f);

    Color color = {0.1f, 0.6f, 1.0f, 0.15f};
    color.alpha *= brightness;
    renderer.drawEllipse(position, Vector2{48.0f, 38.0f} * pulse, color, ShapeFill::Filled, 32);
    color = Color{0.25f, 0.8f, 1.0f, 0.75f};
    color.alpha *= brightness;
    renderer.drawEllipse(position, Vector2{34.0f, 28.0f}, color, ShapeFill::Outline);
    color = Color{0.75f, 0.3f, 1.0f, 0.8f};
    color.alpha *= brightness;
    renderer.drawEllipse(position, Vector2{25.0f, 20.0f}, color, ShapeFill::Outline);
    renderer.drawEllipse(position, Vector2{14.0f, 12.0f}, Color{0.01f, 0.02f, 0.06f, 1.0f}, ShapeFill::Filled, 32);
}

PortalPair::PortalPair()
    : portals{
        Portal(Vector2{138.0f, 184.0f}, Vector2{738.0f, 432.0f}),
        Portal(Vector2{690.0f, 432.0f}, Vector2{90.0f, 184.0f})
    }
{
}

void PortalPair::update(Shuttle& shuttle, float time)
{
    constexpr float CycleSeconds = 24.0f;
    constexpr float OpeningTimeSeconds = 7.0f;
    constexpr float ClosingTimeSeconds = 14.0f;
    const float cycle = std::fmod(time, CycleSeconds);
    open = cycle > OpeningTimeSeconds && cycle < ClosingTimeSeconds;
    if (!open || time < nextPortalTime) {
        return;
    }
    for (const Portal& portal : portals) {
        if (portal.tryTeleport(shuttle)) {
            constexpr float TeleportCooldownSeconds = 1.0f;
            nextPortalTime = time + TeleportCooldownSeconds;
            return;
        }
    }
}

void PortalPair::draw(NeuexpEngine::Renderer2D& renderer, float time) const
{
    float portalTime = time;
    for (const Portal& portal : portals) {
        portal.draw(renderer, portalTime, open);
        portalTime += 1.0f;
    }
}

Vector2 Station::position() const
{
    return StationPosition;
}

Vector2 Station::beaconPosition(float time) const
{
    const Transform2 orbit = {StationPosition, 0, Vector2{105.0f, 42.0f}, Vector2{}};
    return orbit.toAffine().transformPoint(NeuexpEngine::rotated(Vector2{1, 0}, time * 0.9f));
}

bool Station::locksTarget(Vector2 point) const
{
    return NeuexpEngine::contains(Circle{StationPosition, 72.0f}, point);
}

void Station::draw(NeuexpEngine::Renderer2D& renderer, float time) const
{
    renderer.drawEllipse(StationPosition, Vector2{102.0f, 42.0f}, Color{0.12f, 0.8f, 1.0f, 0.28f}, ShapeFill::Outline);
    renderer.drawCircle(Circle{StationPosition, 32.0f}, Color{0.06f, 0.15f, 0.28f, 1.0f}, ShapeFill::Filled, 32);
    renderer.drawCircle(Circle{StationPosition, 33.0f}, Color{0.15f, 0.85f, 1.0f, 1.0f}, ShapeFill::Outline);
    Color reactorColor = {0.28f, 0.95f, 1.0f, 1.0f};
    reactorColor.alpha = 0.7f + 0.3f * std::sin(time * 2.0f);
    renderer.drawCircle(Circle{StationPosition, 14.0f}, reactorColor, ShapeFill::Filled, 12);

    constexpr int ModuleCount = 4;
    constexpr int ModuleColumns = 2;
    constexpr Vector2 ModuleSpacing = {70.0f, 27.0f};
    const int moduleRows = ModuleCount / ModuleColumns;
    const float firstModuleX = StationPosition.x - static_cast<float>(ModuleColumns - 1) * 0.5f * ModuleSpacing.x;
    float moduleY = StationPosition.y - static_cast<float>(moduleRows - 1) * 0.5f * ModuleSpacing.y;
    int module = 0;
    while (module < ModuleCount) {
        float moduleX = firstModuleX;
        for (int column = 0; column < ModuleColumns && module < ModuleCount; ++column) {
            const Vector2 center = {moduleX, moduleY};
            constexpr Vector2 ModuleRadii = {18.0f, 9.0f};
            renderer.drawEllipse(center, ModuleRadii, Color{0.1f, 0.3f, 0.43f, 1.0f}, ShapeFill::Filled, 8);
            renderer.drawEllipse(center, ModuleRadii, Color{0.25f, 0.82f, 1.0f, 1.0f}, ShapeFill::Outline, 8);
            renderer.drawCircle(Circle{center, 3.0f}, Color{1.0f, 0.66f, 0.23f, 1.0f}, ShapeFill::Filled, 8);
            moduleX += ModuleSpacing.x;
            ++module;
        }
        moduleY += ModuleSpacing.y;
    }

    const Vector2 beacon = beaconPosition(time);
    renderer.drawCircle(Circle{beacon, 14.0f}, Color{1.0f, 0.48f, 0.18f, 0.3f}, ShapeFill::Filled, 32);
    renderer.drawCircle(Circle{beacon, 6.0f}, Color{1.0f, 0.6f, 0.22f, 1.0f}, ShapeFill::Filled, 12);
}

void Background::draw(NeuexpEngine::Renderer2D& renderer, Size2 worldSize, float time) const
{
    drawNebula(renderer, time);
    drawStars(renderer, worldSize, time);
    drawPlanet(renderer, time);
}

void Background::drawNebula(NeuexpEngine::Renderer2D& renderer, float time) const
{
    constexpr int NebulaCloudCount = 6;
    constexpr Vector2 NebulaStart = {80.0f, 290.0f};

    float x = NebulaStart.x;
    float phase = time * 0.1f;
    for (int cloud = 0; cloud < NebulaCloudCount; ++cloud) {
        const float y = NebulaStart.y + 30.0f * std::sin(phase);
        renderer.drawEllipse(Vector2{x, y}, Vector2{150.0f, 64.0f}, Color{0.18f, 0.17f, 0.55f, 0.06f}, ShapeFill::Filled, 32);
        x += 110.0f;
        phase += 1.0f;
    }
}

void Background::drawStars(NeuexpEngine::Renderer2D& renderer, Size2 worldSize, float time) const
{
    constexpr unsigned int StarCount = 130u;

    DrawOptions options;
    const unsigned int screenWidth = static_cast<unsigned int>(worldSize.width);
    const unsigned int screenHeight = static_cast<unsigned int>(worldSize.height);

    options.pointSize = 2.0f;
    options.color = Color{0.6f, 0.75f, 1.0f, 1.0f};
    for (unsigned int star = 0; star < StarCount; ++star) {
        const unsigned int seed = star * 7919u + 17u;
        const float x = static_cast<float>((seed * 37u + 91u) % screenWidth);
        const float y = static_cast<float>((seed * 53u + 29u) % screenHeight);
        const float glow = 0.65f + 0.35f * std::sin(time + static_cast<float>(star));
        options.color.alpha = glow;
        renderer.drawPoint(Vector2{x, y}, options);
    }
}

void Background::drawPlanet(NeuexpEngine::Renderer2D& renderer, float time) const
{
    constexpr Vector2 PlanetPosition = {655.0f, 340.0f};
    constexpr int PlanetBandCount = 7;

    renderer.drawCircle(Circle{PlanetPosition, 145.0f}, Color{0.15f, 0.65f, 1.0f, 0.16f}, ShapeFill::Filled, 32);
    renderer.drawCircle(Circle{PlanetPosition, 126.0f}, Color{0.08f, 0.28f, 0.58f, 1.0f}, ShapeFill::Filled, 32);

    const int firstBand = -PlanetBandCount / 2;
    const float firstBandOffset = static_cast<float>(firstBand);
    float bandY = PlanetPosition.y + firstBandOffset * 27.0f;
    float bandPhase = time + firstBandOffset;
    DrawOptions options;
    options.lineWidth = 1.0f;
    options.color = Color{0.35f, 0.72f, 1.0f, 0.2f};
    for (int band = 0; band < PlanetBandCount; ++band) {
        options.color.alpha = 0.2f + 0.08f * std::sin(bandPhase);
        constexpr float PlanetBandHalfWidth = 115.0f;
        renderer.drawLine(Vector2{PlanetPosition.x - PlanetBandHalfWidth, bandY},
            Vector2{PlanetPosition.x + PlanetBandHalfWidth, bandY}, options);
        bandY += 27.0f;
        bandPhase += 1.0f;
    }

    options.lineWidth = 6.0f;
    options.color = Color{0.3f, 0.8f, 1.0f, 0.55f};
    renderer.drawEllipse(PlanetPosition, Vector2{175.0f, 49.0f}, options, ShapeFill::Outline);
    options.lineWidth = 1.0f;
    options.color = Color{0.8f, 0.92f, 1.0f, 0.7f};
    renderer.drawEllipse(PlanetPosition, Vector2{191.0f, 55.0f}, options, ShapeFill::Outline);
}
}
