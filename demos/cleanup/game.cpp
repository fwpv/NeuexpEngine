#include "game.h"
#include "core/geometry.h"
#include "core/math2d.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace Cleanup
{
World::World()
    : bounds{0, 0, 3000, 2100}
    , homeBounds{2580, 1650, 240, 180}
    , homeLotBounds{2500, 1570, 400, 380}
    , puddles{
        Puddle{Circle{Vector2{1280, 1050}, 80}, true}, Puddle{Circle{Vector2{1740, 1050}, 85}, false},
        Puddle{Circle{Vector2{700, 600}, 95}, true}, Puddle{Circle{Vector2{2220, 620}, 100}, false},
        Puddle{Circle{Vector2{2200, 1500}, 90}, true}, Puddle{Circle{Vector2{780, 1500}, 95}, false}
    }
    , obstacles{
        Rect{1270, 780, 230, 110}, Rect{510, 120, 240, 150}, Rect{1520, 1500, 240, 140},
        Rect{890, 1750, 180, 190}, Rect{2290, 240, 240, 190}, Rect{900, 210, 240, 210},
        Rect{210, 420, 130, 110}, Rect{560, 770, 210, 180}, Rect{1390, 150, 230, 130},
        Rect{350, 1670, 230, 200}, Rect{1930, 590, 110, 240}, Rect{1350, 490, 120, 150},
        Rect{2730, 640, 130, 200}, Rect{120, 1510, 140, 220}, Rect{2650, 970, 210, 150},
        Rect{660, 1220, 300, 80}, Rect{2430, 530, 150, 230}, Rect{1880, 1160, 110, 200},
        Rect{1430, 1760, 140, 150}, Rect{1110, 1320, 100, 210}, Rect{2190, 850, 310, 90},
        Rect{430, 1090, 160, 170}, Rect{1860, 150, 110, 210}, Rect{180, 1090, 120, 120},
        Rect{1540, 510, 200, 190}, Rect{1640, 1860, 170, 100}, Rect{290, 640, 160, 230},
        Rect{1300, 1270, 110, 190}, Rect{2440, 1260, 240, 160}, Rect{250, 240, 120, 90},
        Rect{1840, 1540, 120, 240}, Rect{980, 940, 160, 130}, Rect{2190, 1160, 110, 170},
        Rect{1220, 1850, 110, 120}, Rect{400, 1330, 180, 190}, Rect{2160, 1680, 100, 80},
        Rect{880, 690, 280, 120}, Rect{2730, 110, 120, 160}, Rect{1490, 1210, 180, 170},
        Rect{2040, 230, 170, 150}, Rect{2160, 1830, 100, 90}, Rect{1290, 1560, 160, 110}
    }
{
}

namespace
{
bool blocked(const World& world, Vector2 point, float radius, bool homeLocked)
{
    const Circle body = {point, radius};
    if (homeLocked && NeuexpEngine::intersects(world.homeBounds, body)) {
        return true;
    }
    return std::any_of(world.obstacles.begin(), world.obstacles.end(), [&](const Rect& obstacle) {
        return NeuexpEngine::intersects(obstacle, body);
    });
}

bool canPlaceTrash(const World& world, const Rules& rules, Vector2 point)
{
    const float clearance = rules.trashRadius + rules.playerRadius;
    return !blocked(world, point, clearance, true)
        && std::none_of(world.puddles.begin(), world.puddles.end(), [&](const Puddle& puddle) {
            return NeuexpEngine::intersects(puddle.shape, Circle{point, clearance});
        });
}

Vector2 movePlayer(const World& world, const Rules& rules, Vector2 position, Vector2 movement, bool homeLocked)
{
    const Rect playerBounds = {rules.playerRadius, rules.playerRadius,
        world.bounds.size.width - 2 * rules.playerRadius, world.bounds.size.height - 2 * rules.playerRadius};
    // Короткие шаги исключают проход сквозь препятствие; раздельные оси дают скольжение вдоль стен.
    const int steps = std::max(1, static_cast<int>(std::ceil(NeuexpEngine::length(movement) / (rules.playerRadius / 4))));
    const Vector2 step = movement / static_cast<float>(steps);
    for (int index = 0; index < steps; ++index) {
        Vector2 next = NeuexpEngine::closestPoint(playerBounds, position + Vector2{step.x, 0});
        if (!blocked(world, next, rules.playerRadius, homeLocked)) {
            position = next;
        }
        next = NeuexpEngine::closestPoint(playerBounds, position + Vector2{0, step.y});
        if (!blocked(world, next, rules.playerRadius, homeLocked)) {
            position = next;
        }
    }
    return position;
}
}

GameState::GameState()
    : player(world.bounds.center())
    , nextDropTime(rules.glueLifetime)
{
    // Первые предметы расположены рядом с начальной оранжевой лужей.
    trash[0].position = Vector2{1410, 1050};
    trash[1].position = Vector2{1450, 1100};
    trash[2].position = Vector2{1370, 1150};
    int candidate = 3;
    for (int index = 3; index < static_cast<int>(trash.size()); ++index) {
        Vector2 point;
        do {
            point = Vector2{
                160.0F + static_cast<float>((candidate * 463) % 2680),
                150.0F + static_cast<float>((candidate * 347) % 1800)
            };
            ++candidate;
        } while (!canPlaceTrash(world, rules, point));
        trash[static_cast<std::size_t>(index)].position = point;
    }
}

const GameState& Game::state() const
{
    return currentState;
}

void Game::setTarget(Vector2 position)
{
    const auto& rules = currentState.rules;
    const auto& bounds = currentState.world.bounds;
    currentState.target = NeuexpEngine::closestPoint(Rect{rules.playerRadius, rules.playerRadius,
        bounds.size.width - 2 * rules.playerRadius, bounds.size.height - 2 * rules.playerRadius}, position);
}

void Game::update(Vector2 direction, float deltaTime)
{
    GameState& game = currentState;
    if (game.outcome != Outcome::Playing) {
        return;
    }
    game.feedbackTime = std::max(0.0F, game.feedbackTime - deltaTime);
    game.motivationDecayTime += deltaTime;
    while (game.motivation > game.rules.passiveMotivationFloor && game.motivationDecayTime >= game.rules.motivationDecayInterval) {
        --game.motivation;
        game.motivationDecayTime -= game.rules.motivationDecayInterval;
    }
    if (game.motivation <= game.rules.passiveMotivationFloor) {
        // Потеря груза может опустить мотивацию ниже порога; время её не восстанавливает.
        game.motivationDecayTime = 0;
    }
    for (Trash& item : game.trash) {
        item.pickupDelay = std::max(0.0F, item.pickupDelay - deltaTime);
    }

    constexpr float MovementSpeed = 260.0F; // Мировые единицы в секунду без груза.
    constexpr float FullCargoSlowdown = 0.4F;
    const float speed = MovementSpeed * (1 - FullCargoSlowdown * static_cast<float>(game.carried) / static_cast<float>(game.rules.glueCapacity));
    const Vector2 previousPosition = game.player;
    Vector2 movement;
    if (direction != Vector2{}) {
        game.target.reset();
        direction = NeuexpEngine::normalized(direction);
    } else if (game.target) {
        const Vector2 offset = *game.target - game.player;
        if (NeuexpEngine::length(offset) <= speed * deltaTime) {
            movement = offset;
            game.target.reset();
        } else {
            direction = NeuexpEngine::normalized(offset);
        }
    }
    if (direction != Vector2{}) {
        movement = direction * speed * deltaTime;
    }
    game.player = movePlayer(game.world, game.rules, game.player, movement, game.washed < game.rules.homeGoal);
    if (movement != Vector2{}) {
        game.heading = std::atan2(movement.y, movement.x);
    }
    if (game.player == previousPosition && movement != Vector2{}) {
        game.target.reset();
    }

    bool inOrange = false;
    bool inBlue = false;
    for (const Puddle& puddle : game.world.puddles) {
        if (NeuexpEngine::intersects(puddle.shape, Circle{game.player, game.rules.playerRadius})) {
            inOrange = inOrange || puddle.sticky;
            inBlue = inBlue || !puddle.sticky;
        }
    }
    if (inBlue) {
        if (game.carried > 0) {
            for (Trash& item : game.trash) {
                if (item.state == TrashState::Carried) {
                    item.state = TrashState::Washed;
                }
            }
            game.lastDelivery = game.carried;
            constexpr int BonusCargoThreshold = 5;
            const int reward = game.carried * game.rules.deliveryReward + (game.carried >= BonusCargoThreshold ? game.carried : 0);
            const int previousMotivation = game.motivation;
            game.motivation = std::min(game.rules.maximumMotivation, game.motivation + reward);
            game.motivationChange = game.motivation - previousMotivation;
            constexpr float DeliveryFeedbackSeconds = 2;
            game.feedbackTime = DeliveryFeedbackSeconds;
            game.washed += game.carried;
            game.carried = 0;
        }
        game.coat = Coat::Clean;
        game.glue = 0;
        game.cargoAge = 0;
        game.nextDropTime = game.rules.glueLifetime;
    } else if (inOrange && game.coat != Coat::Sticky) {
        game.coat = Coat::Sticky;
        game.glue = game.rules.glueCapacity;
        game.cargoAge = 0;
        game.nextDropTime = game.rules.glueLifetime;
    }

    if (game.coat == Coat::Sticky && game.glue > 0 && game.cargoAge < game.rules.glueLifetime) {
        const float pickupRadius = game.rules.playerRadius + static_cast<float>(game.carried);
        for (Trash& item : game.trash) {
            if (game.glue == 0) {
                break;
            }
            if (item.state == TrashState::Ground && item.pickupDelay == 0
                && NeuexpEngine::intersects(Circle{game.player, pickupRadius}, Circle{item.position, game.rules.trashRadius})) {
                item.state = TrashState::Carried;
                // Золотой угол в радианах разносит груз вокруг игрока.
                constexpr float AttachmentAngleStep = 2.39996323F;
                item.attachment = NeuexpEngine::rotated(Vector2{game.rules.playerRadius + 9.0f + 4.0f * std::sqrt(static_cast<float>(game.carried)), 0},
                    static_cast<float>(game.carried) * AttachmentAngleStep);
                ++game.carried;
                --game.glue;
            }
        }
    }

    if (game.carried > 0) {
        game.cargoAge += deltaTime;
        while (game.cargoAge >= game.nextDropTime && game.carried > 0) {
            for (Trash& item : game.trash) {
                if (item.state == TrashState::Carried) {
                    item.state = TrashState::Ground;
                    item.position = NeuexpEngine::closestPoint(Rect{game.rules.trashRadius, game.rules.trashRadius,
                        game.world.bounds.size.width - 2 * game.rules.trashRadius, game.world.bounds.size.height - 2 * game.rules.trashRadius},
                        game.player + NeuexpEngine::rotated(item.attachment, game.heading));
                    // Упавший предмет остаётся доступен для повторного сбора.
                    if (blocked(game.world, item.position, game.rules.trashRadius + game.rules.playerRadius, game.washed < game.rules.homeGoal)) {
                        item.position = game.player;
                    }
                    for (const Puddle& puddle : game.world.puddles) {
                        if (!puddle.sticky && NeuexpEngine::intersects(puddle.shape, Circle{item.position, game.rules.trashRadius})) {
                            item.position = game.player;
                        }
                    }
                    constexpr float PickupDelaySeconds = 4;
                    item.pickupDelay = PickupDelaySeconds;
                    --game.carried;
                    break;
                }
            }
            const int previousMotivation = game.motivation;
            game.motivation = std::max(0, game.motivation - game.rules.dropPenalty);
            game.motivationChange = game.motivation - previousMotivation;
            constexpr float DropFeedbackSeconds = 2;
            constexpr float DropIntervalSeconds = 3.0F;
            game.feedbackTime = DropFeedbackSeconds;
            game.nextDropTime += DropIntervalSeconds;
            if (game.motivation == 0) {
                game.outcome = Outcome::Lost;
                game.target.reset();
                return;
            }
        }
    }
    if (game.washed >= game.rules.homeGoal && NeuexpEngine::contains(game.world.homeBounds, game.player)) {
        game.outcome = Outcome::Won;
        game.target.reset();
    }
}
}
