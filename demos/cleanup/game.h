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
inline constexpr Rect HomeLotBounds = { 2500, 1570, 400, 380 };
inline constexpr float PlayerRadius = 18;
inline constexpr float TrashRadius = 8;
inline constexpr float PlayerSpeed = 260;
inline constexpr float GoldenAngle = 2.39996323F;
inline constexpr int TrashCount = 60;
inline constexpr int HomeGoal = 40;
inline constexpr int GlueCapacity = 10;
inline constexpr int MaximumMotivation = 100;
inline constexpr int PassiveMotivationFloor = 10;
inline constexpr float MotivationDecayInterval = 2;
inline constexpr int DropPenalty = 8;
inline constexpr int DeliveryReward = 4;
inline constexpr float GlueLifetime = 15;
inline constexpr float GlueWarningTime = 10;
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
    { { { 700, 600 }, 95 }, true }, { { { 2220, 620 }, 100 }, false },
    { { { 2200, 1500 }, 90 }, true }, { { { 780, 1500 }, 95 }, false }
}};

// Проходы между препятствиями остаются шире диаметра персонажа.
inline constexpr std::array<Rect, 42> Obstacles = {{
    { 1270, 780, 230, 110 }, { 510, 120, 240, 150 }, { 1520, 1500, 240, 140 },
    { 890, 1750, 180, 190 }, { 2290, 240, 240, 190 }, { 900, 210, 240, 210 },
    { 210, 420, 130, 110 }, { 560, 770, 210, 180 }, { 1390, 150, 230, 130 },
    { 350, 1670, 230, 200 }, { 1930, 590, 110, 240 }, { 1350, 490, 120, 150 },
    { 2730, 640, 130, 200 }, { 120, 1510, 140, 220 }, { 2650, 970, 210, 150 },
    { 660, 1220, 300, 80 }, { 2430, 530, 150, 230 }, { 1880, 1160, 110, 200 },
    { 1430, 1760, 140, 150 }, { 1110, 1320, 100, 210 }, { 2190, 850, 310, 90 },
    { 430, 1090, 160, 170 }, { 1860, 150, 110, 210 }, { 180, 1090, 120, 120 },
    { 1540, 510, 200, 190 }, { 1640, 1860, 170, 100 }, { 290, 640, 160, 230 },
    { 1300, 1270, 110, 190 }, { 2440, 1260, 240, 160 }, { 250, 240, 120, 90 },
    { 1840, 1540, 120, 240 }, { 980, 940, 160, 130 }, { 2190, 1160, 110, 170 },
    { 1220, 1850, 110, 120 }, { 400, 1330, 180, 190 }, { 2160, 1680, 100, 80 },
    { 880, 690, 280, 120 }, { 2730, 110, 120, 160 }, { 1490, 1210, 180, 170 },
    { 2040, 230, 170, 150 }, { 2160, 1830, 100, 90 }, { 1290, 1560, 160, 110 }
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
    float motivationDecayTime = 0;
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
