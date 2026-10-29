#pragma once

#include "entities.h"
#include "math2d.h"
#include "renderer.h"

#include <vector>

namespace Game
{
struct HudState
{
    Vector2 shuttlePosition;
    Vector2 stationPosition;
    Vector2 beaconPosition;
    int score;
    bool targetLocked;
};

class Hud
{
public:
    void draw(Engine::Renderer2D& renderer, const HudState& state, float time) const;

private:
    void drawFrame(Engine::Renderer2D& renderer) const;
    void drawRadar(Engine::Renderer2D& renderer, const HudState& state, float time) const;
    void drawStatusBars(Engine::Renderer2D& renderer) const;
    void drawScore(Engine::Renderer2D& renderer, int score) const;
    void drawTarget(Engine::Renderer2D& renderer, Vector2 position, bool locked) const;
};

class Scene
{
public:
    Scene();
    Size2 size() const;
    void update(const InputState& input, float time, float deltaTime);
    void drawWorld(Engine::Renderer2D& renderer, float time) const;
    void drawHud(Engine::Renderer2D& renderer, float time) const;

private:
    void updateBolts(float time, float deltaTime);

    Background background;
    Station station;
    PortalPair portals;
    Shuttle shuttle;
    std::vector<Drone> drones;
    std::vector<Bolt> bolts;
    Hud hud;
    int score;
};
}
