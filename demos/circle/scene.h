#pragma once

#include "entities.h"
#include "core/math2d.h"
#include "graphics/renderer.h"

#include <vector>

namespace Game
{
struct HudState
{
    Vector2 shuttlePosition;
    Vector2 stationPosition;
    Vector2 beaconPosition;
    int score = 0;
    bool targetLocked = false;
};

class Hud
{
public:
    void draw(NeuexpEngine::Renderer2D& renderer, const HudState& state, float time) const;

private:
    void drawFrame(NeuexpEngine::Renderer2D& renderer) const;
    void drawRadar(NeuexpEngine::Renderer2D& renderer, const HudState& state, float time) const;
    void drawStatusBars(NeuexpEngine::Renderer2D& renderer) const;
    void drawScore(NeuexpEngine::Renderer2D& renderer, int score) const;
    void drawTarget(NeuexpEngine::Renderer2D& renderer, Vector2 position, bool locked) const;
};

class Scene
{
public:
    Scene();
    Size2 size() const;
    void update(const InputState& input, float time, float deltaTime);
    void drawWorld(NeuexpEngine::Renderer2D& renderer, float time) const;
    void drawHud(NeuexpEngine::Renderer2D& renderer, float time) const;

private:
    void updateBolts(float time, float deltaTime);

    Background background;
    Station station;
    PortalPair portals;
    Shuttle shuttle;
    std::vector<Drone> drones;
    std::vector<Bolt> bolts;
    Hud hud;
    int score = 0;
};
}
