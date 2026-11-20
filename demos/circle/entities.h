#pragma once

#include "core/geometry.h"
#include "graphics/renderer.h"
#include "core/math2d.h"
#include "core/transform.h"

#include <array>
#include <optional>

namespace Game
{
using NeuexpEngine::Affine2;
using NeuexpEngine::Circle;
using NeuexpEngine::Color;
using NeuexpEngine::Pi;
using NeuexpEngine::Rect;
using NeuexpEngine::Size2;
using NeuexpEngine::Transform2;
using NeuexpEngine::Vector2;

using NeuexpEngine::DrawOptions;
using NeuexpEngine::ShapeFill;

// Игровые действия текущего кадра без системных кодов клавиш.
struct InputState
{
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool fire = false;
};

class Bolt
{
public:
    Bolt(Vector2 start, Vector2 speed, float lifetime);
    void update(float deltaTime);
    bool isExpired(const Rect& bounds) const;
    Vector2 position() const;
    void draw(NeuexpEngine::Renderer2D& renderer) const;

private:
    Vector2 location;
    Vector2 velocity;
    float secondsLeft;
};

class Shuttle
{
public:
    void update(const InputState& input, float deltaTime);
    std::optional<Bolt> fire(bool requested, float time);
    Vector2 position() const;
    void teleport(Vector2 destination);
    void draw(NeuexpEngine::Renderer2D& renderer, float time) const;

private:
    Transform2 transform = {Vector2{170, 390}, Pi / 2, Vector2{1, 1}, Vector2{}};
    bool thrusting = false;
    float nextShotTime = 0;
};

class Drone
{
public:
    explicit Drone(float startPhase);
    void update(float time);
    bool tryHit(Vector2 point, float time);
    void draw(NeuexpEngine::Renderer2D& renderer, float time) const;

private:
    Vector2 position(float time) const;

    float phase;
    float respawnTime = 0;
    bool active = true;
};

class Portal
{
public:
    Portal(Vector2 entrance, Vector2 exit);
    bool tryTeleport(Shuttle& shuttle) const;
    void draw(NeuexpEngine::Renderer2D& renderer, float time, bool open) const;

private:
    Vector2 position;
    Vector2 destination;
};

class PortalPair
{
public:
    PortalPair();
    void update(Shuttle& shuttle, float time);
    void draw(NeuexpEngine::Renderer2D& renderer, float time) const;

private:
    const std::array<Portal, 2> portals;
    float nextPortalTime = 0;
    bool open = false;
};

class Station
{
public:
    Vector2 position() const;
    Vector2 beaconPosition(float time) const;
    bool locksTarget(Vector2 point) const;
    void draw(NeuexpEngine::Renderer2D& renderer, float time) const;
};

class Background
{
public:
    void draw(NeuexpEngine::Renderer2D& renderer, Size2 worldSize, float time) const;

private:
    void drawNebula(NeuexpEngine::Renderer2D& renderer, float time) const;
    void drawStars(NeuexpEngine::Renderer2D& renderer, Size2 worldSize, float time) const;
    void drawPlanet(NeuexpEngine::Renderer2D& renderer, float time) const;
};
}
