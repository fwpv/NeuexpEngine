#pragma once

#include "geometry.h"

#include <array>
#include <optional>

namespace Cleanup
{
using Engine::Circle;
using Engine::Rect;
using Engine::Vector2;

inline constexpr Rect WorldBounds = { 0, 0, 3000, 2100 };
inline constexpr Rect HomeBounds = { 2580, 1650, 240, 180 };
inline constexpr float PlayerRadius = 18;
inline constexpr float TrashRadius = 8;
inline constexpr float PlayerSpeed = 260;
inline constexpr float GoldenAngle = 2.39996323F;
inline constexpr int TrashCount = 60;
inline constexpr int HomeGoal = 40;
inline constexpr int GlueCapacity = 10;
inline constexpr int MaximumMotivation = 100;
inline constexpr int DropPenalty = 8;
inline constexpr int DeliveryReward = 4;
inline constexpr float GlueLifetime = 30;
inline constexpr float GlueWarningTime = 25;
inline constexpr float DropInterval = 3;
inline constexpr float DroppedPickupDelay = 4;
inline constexpr float MaximumSlowdown = 0.4F;

struct Puddle
{
    Circle shape;
    bool sticky;
};

inline constexpr std::array<Puddle, 6> Puddles = {{
    { { { 1280, 1050 }, 80 }, true }, { { { 1740, 1050 }, 85 }, false },
    { { { 520, 480 }, 95 }, true }, { { { 2420, 490 }, 100 }, false },
    { { { 2440, 1680 }, 90 }, true }, { { { 560, 1660 }, 95 }, false }
}};

enum class Coat
{
    Dry,
    Sticky,
    Clean
};

enum class TrashState
{
    Ground,
    Carried,
    Washed
};

enum class Outcome
{
    Playing,
    Won,
    Lost
};

struct Trash
{
    Vector2 position;
    Vector2 attachment;
    TrashState state = TrashState::Ground;
    float pickupDelay = 0;
};

struct Game
{
    Vector2 player = WorldBounds.center();
    float heading = 0;
    Coat coat = Coat::Dry;
    std::array<Trash, TrashCount> trash;
    int carried = 0;
    int washed = 0;
    int glue = 0;
    int motivation = 60;
    float cargoAge = 0;
    float nextDropTime = GlueLifetime;
    float feedbackTime = 0;
    int motivationChange = 0;
    int lastDelivery = 0;
    Outcome outcome = Outcome::Playing;
    std::optional<Vector2> target;

    Game();
    void update(Vector2 direction, float deltaTime);
};
}
