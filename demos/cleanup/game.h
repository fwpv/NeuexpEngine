#pragma once

#include "geometry.h"

#include <array>
#include <optional>

namespace Cleanup
{
using Engine::Circle;
using Engine::Rect;
using Engine::Vector2;

struct Puddle
{
    Circle shape;
    bool sticky;
};

struct World
{
    World();

    Rect bounds;
    Rect homeBounds;
    Rect homeLotBounds;
    std::array<Puddle, 6> puddles;
    std::array<Rect, 42> obstacles;
};

struct Rules
{
    Rules();

    float playerRadius;
    float trashRadius;
    int homeGoal;
    int glueCapacity;
    int maximumMotivation;
    int passiveMotivationFloor;
    float motivationDecayInterval;
    int dropPenalty;
    int deliveryReward;
    float glueLifetime;
    float glueWarningTime;
};

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
    Trash();

    Vector2 position;
    Vector2 attachment;
    TrashState state;
    float pickupDelay;
};

struct Game
{
    Game();
    void update(Vector2 direction, float deltaTime);

    World world;
    Rules rules;
    Vector2 player;
    float heading;
    Coat coat;
    std::array<Trash, 60> trash;
    int carried;
    int washed;
    int glue;
    int motivation;
    float motivationDecayTime;
    float cargoAge;
    float nextDropTime;
    float feedbackTime;
    int motivationChange;
    int lastDelivery;
    Outcome outcome;
    std::optional<Vector2> target;
};
}
