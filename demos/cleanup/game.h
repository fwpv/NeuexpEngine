#pragma once

#include "core/geometry.h"
#include "core/math2d.h"

#include <array>
#include <optional>

namespace Cleanup
{
using NeuexpEngine::Circle;
using NeuexpEngine::Rect;
using NeuexpEngine::Vector2;

struct Puddle
{
    Circle shape;
    bool sticky = false;
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
    float playerRadius = 18;
    float trashRadius = 8;
    int homeGoal = 40;
    int glueCapacity = 10;
    int maximumMotivation = 100;
    int passiveMotivationFloor = 10;
    float motivationDecayInterval = 2;
    int dropPenalty = 8;
    int deliveryReward = 4;
    float glueLifetime = 15;
    float glueWarningTime = 10;
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
    Vector2 position;
    Vector2 attachment;
    TrashState state = TrashState::Ground;
    float pickupDelay = 0;
};

struct GameState
{
    GameState();

    World world;
    Rules rules;
    Vector2 player;
    float heading = 0;
    Coat coat = Coat::Dry;
    std::array<Trash, 60> trash;
    int carried = 0;
    int washed = 0;
    int glue = 0;
    int motivation = 60;
    float motivationDecayTime = 0;
    float cargoAge = 0;
    float nextDropTime;
    float feedbackTime = 0;
    int motivationChange = 0;
    int lastDelivery = 0;
    Outcome outcome = Outcome::Playing;
    std::optional<Vector2> target;
};

class Game
{
public:
    const GameState& state() const;
    void setTarget(Vector2 position);
    void update(Vector2 direction, float deltaTime);

private:
    GameState currentState;
};
}
