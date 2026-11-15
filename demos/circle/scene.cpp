#include "scene.h"
#include "entities.h"
#include "geometry.h"
#include "math2d.h"
#include "renderer.h"
#include "transform.h"

namespace Game
{
namespace
{
constexpr Size2 WorldSize = {800, 600};
constexpr float HudInactiveAlpha = 0.18f;
}

Scene::Scene()
{
    constexpr int StartingDroneCount = 3;

    float phase = 0;
    for (int drone = 0; drone < StartingDroneCount; ++drone) {
        drones.emplace_back(phase);
        phase += 2.094f;
    }
}

Size2 Scene::size() const
{
    return WorldSize;
}

void Scene::update(const InputState& input, float time, float deltaTime)
{
    // Одно игровое время сохраняет общую паузу движения, стрельбы и порталов.
    shuttle.update(input, deltaTime);
    if (const auto bolt = shuttle.fire(input.fire, time)) {
        bolts.push_back(*bolt);
    }
    for (Drone& drone : drones) {
        drone.update(time);
    }
    updateBolts(time, deltaTime);
    portals.update(shuttle, time);
}

void Scene::updateBolts(float time, float deltaTime)
{
    for (auto bolt = bolts.begin(); bolt != bolts.end();) {
        bolt->update(deltaTime);
        bool hit = false;
        for (Drone& drone : drones) {
            if (drone.tryHit(bolt->position(), time)) {
                hit = true;
                ++score;
                break;
            }
        }
        if (hit || bolt->isExpired(Rect{Vector2{}, WorldSize})) {
            bolt = bolts.erase(bolt);
        } else {
            ++bolt;
        }
    }
}

void Scene::drawWorld(NeuexpEngine::Renderer2D& renderer, float time) const
{
    renderer.drawGradientRectangle(Rect{Vector2{}, WorldSize}, Color{0.025f, 0.04f, 0.12f, 1.0f}, Color{0.005f, 0.009f, 0.028f, 1.0f});
    // Дальние объекты рисуются раньше ближайших.
    background.draw(renderer, WorldSize, time);
    station.draw(renderer, time);
    portals.draw(renderer, time);
    for (const Drone& drone : drones) {
        drone.draw(renderer, time);
    }
    for (const Bolt& bolt : bolts) {
        bolt.draw(renderer);
    }
    shuttle.draw(renderer, time);
}

void Scene::drawHud(NeuexpEngine::Renderer2D& renderer, float time) const
{
    const HudState state = {shuttle.position(), station.position(), station.beaconPosition(time),
        score, station.locksTarget(shuttle.position())};
    hud.draw(renderer, state, time);
}

void Hud::drawFrame(NeuexpEngine::Renderer2D& renderer) const
{
    constexpr Color HudFrameColor = {0.15f, 0.72f, 0.92f, 0.55f};
    constexpr Rect Frame = {22, 22, 756, 500};

    const Vector2 start = Frame.position;
    const Vector2 end = start + Vector2{Frame.size.width, Frame.size.height};
    renderer.drawLine(start, Vector2{end.x, start.y}, HudFrameColor);
    renderer.drawLine(Vector2{start.x, end.y}, end, HudFrameColor);
    constexpr float FrameCornerLength = 50.0f;
    renderer.drawLine(start, start + Vector2{0, FrameCornerLength}, HudFrameColor);
    renderer.drawLine(Vector2{end.x, start.y}, Vector2{end.x, start.y + FrameCornerLength}, HudFrameColor);
}

void Hud::drawRadar(NeuexpEngine::Renderer2D& renderer, const HudState& state, float time) const
{
    constexpr Color RadarColor = {0.2f, 0.65f, 0.82f, 0.7f};
    constexpr Vector2 RadarCenter = {720, 470};

    renderer.drawEllipse(RadarCenter, Vector2{44.0f, 22.0f}, RadarColor, ShapeFill::Outline);
    renderer.drawEllipse(RadarCenter, Vector2{28.0f, 14.0f}, RadarColor, ShapeFill::Outline);
    const Transform2 sweep = {RadarCenter, 0, Vector2{42.0f, 21.0f}, Vector2{}};
    renderer.drawLine(RadarCenter, sweep.toAffine().transformPoint(NeuexpEngine::rotated(Vector2{1, 0}, time)), Color{0.2f, 0.8f, 0.95f, 0.4f});

    // Положение шаттла показывается относительно станции, а не края всего уровня.
    const Affine2 radar = Transform2{RadarCenter, 0,
        Vector2{0.2f, 0.2f}, state.stationPosition}.toAffine();
    const Vector2 shuttleBlip = radar.transformPoint(state.shuttlePosition);
    constexpr float BlipRadius = 2.5f;
    renderer.drawCircle(Circle{shuttleBlip, BlipRadius}, Color{0.3f, 0.95f, 1.0f, 1.0f}, ShapeFill::Filled, 8);
    const Vector2 beaconBlip = radar.transformPoint(state.beaconPosition);
    renderer.drawCircle(Circle{beaconBlip, BlipRadius}, Color{1.0f, 0.6f, 0.22f, 1.0f}, ShapeFill::Filled, 8);
}

void Hud::drawStatusBars(NeuexpEngine::Renderer2D& renderer) const
{
    constexpr int BarCellCount = 12;

    float cellX = 35.0f;
    for (int cell = 0; cell < BarCellCount; ++cell) {
        constexpr float BarCellSize = 9.0f;
        Color shield{0.14f, 0.82f, 0.95f, 0.9f};
        shield.alpha = cell < 10 ? shield.alpha : HudInactiveAlpha;
        renderer.drawRectangle(Rect{cellX, 546.0f, BarCellSize, BarCellSize}, shield);
        Color energy{1.0f, 0.58f, 0.2f, 0.9f};
        energy.alpha = cell < 8 ? energy.alpha : HudInactiveAlpha;
        renderer.drawRectangle(Rect{cellX, 568.0f, BarCellSize, BarCellSize}, energy);
        cellX += 13.0f;
    }
}

void Hud::drawScore(NeuexpEngine::Renderer2D& renderer, int score) const
{
    constexpr int ScoreLightCount = 8;

    float lightX = 694.0f;
    for (int point = 0; point < ScoreLightCount; ++point) {
        Color color{1.0f, 0.32f, 0.28f, 0.95f};
        color.alpha = point < score ? color.alpha : HudInactiveAlpha;
        renderer.drawCircle(Circle{Vector2{lightX, 65.0f}, 3.5f}, color, ShapeFill::Filled, 8);
        lightX += 12.0f;
    }
}

void Hud::drawTarget(NeuexpEngine::Renderer2D& renderer, Vector2 position, bool locked) const
{
    Color color{1.0f, 0.55f, 0.2f, 1.0f};
    color.alpha = locked ? color.alpha : 0.45f;
    renderer.drawCircle(Circle{position, 38.0f}, color, ShapeFill::Outline);
    if (locked) {
        constexpr float TargetMarkInner = 33.0f;
        constexpr float TargetMarkOuter = 48.0f;
        const float x = position.x;
        const float y = position.y;
        renderer.drawLine(Vector2{x - TargetMarkOuter, y}, Vector2{x - TargetMarkInner, y}, color);
        renderer.drawLine(Vector2{x + TargetMarkInner, y}, Vector2{x + TargetMarkOuter, y}, color);
        renderer.drawLine(Vector2{x, y - TargetMarkOuter}, Vector2{x, y - TargetMarkInner}, color);
        renderer.drawLine(Vector2{x, y + TargetMarkInner}, Vector2{x, y + TargetMarkOuter}, color);
    }
}

void Hud::draw(NeuexpEngine::Renderer2D& renderer, const HudState& state, float time) const
{
    drawFrame(renderer);
    drawRadar(renderer, state, time);
    drawStatusBars(renderer);
    drawScore(renderer, state.score);
    drawTarget(renderer, state.shuttlePosition, state.targetLocked);
}
}
