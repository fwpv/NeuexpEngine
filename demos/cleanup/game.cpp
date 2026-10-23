#include "game.h"

#include <algorithm>
#include <cmath>

namespace Cleanup
{
namespace
{
bool blocked(Vector2 point, float radius, bool homeLocked)
{
    const Circle body = { point, radius };
    if (homeLocked && Engine::intersects(HomeBounds, body)) {
        return true;
    }
    return std::any_of(Obstacles.begin(), Obstacles.end(), [&](const Rect& obstacle) {
        return Engine::intersects(obstacle, body);
    });
}

bool canPlaceTrash(Vector2 point)
{
    const float clearance = TrashRadius + PlayerRadius;
    return !blocked(point, clearance, true)
        && std::none_of(Puddles.begin(), Puddles.end(), [&](const Puddle& puddle) {
            return Engine::intersects(puddle.shape, Circle{ point, clearance });
        });
}

Vector2 movePlayer(Vector2 position, Vector2 movement, bool homeLocked)
{
    const Rect playerBounds = { PlayerRadius, PlayerRadius,
        WorldBounds.size.width - 2 * PlayerRadius, WorldBounds.size.height - 2 * PlayerRadius };
    // Короткие шаги исключают проход сквозь препятствие; раздельные оси дают скольжение вдоль стен.
    const int steps = std::max(1, static_cast<int>(std::ceil(Engine::length(movement) / (PlayerRadius / 4))));
    const Vector2 step = movement / static_cast<float>(steps);
    for (int index = 0; index < steps; ++index) {
        Vector2 next = Engine::closestPoint(playerBounds, position + Vector2{ step.x, 0 });
        if (!blocked(next, PlayerRadius, homeLocked)) {
            position = next;
        }
        next = Engine::closestPoint(playerBounds, position + Vector2{ 0, step.y });
        if (!blocked(next, PlayerRadius, homeLocked)) {
            position = next;
        }
    }
    return position;
}
}

Game::Game()
{
    // Первые предметы расположены рядом с начальной оранжевой лужей.
    trash[0].position = { 1410, 1050 };
    trash[1].position = { 1450, 1100 };
    trash[2].position = { 1370, 1150 };
    int candidate = 3;
    for (int index = 3; index < TrashCount; ++index) {
        Vector2 point;
        do {
            point = {
                160.0F + static_cast<float>((candidate * 463) % 2680),
                150.0F + static_cast<float>((candidate * 347) % 1800)
            };
            ++candidate;
        } while (!canPlaceTrash(point));
        trash[static_cast<std::size_t>(index)].position = point;
    }
}

void Game::update(Vector2 direction, float deltaTime)
{
    if (outcome != Outcome::Playing) {
        return;
    }
    feedbackTime = std::max(0.0F, feedbackTime - deltaTime);
    motivationDecayTime += deltaTime;
    while (motivation > PassiveMotivationFloor && motivationDecayTime >= MotivationDecayInterval) {
        --motivation;
        motivationDecayTime -= MotivationDecayInterval;
    }
    if (motivation <= PassiveMotivationFloor) {
        // Потеря груза может опустить мотивацию ниже порога; время её не восстанавливает.
        motivationDecayTime = 0;
    }
    for (Trash& item : trash) {
        item.pickupDelay = std::max(0.0F, item.pickupDelay - deltaTime);
    }

    const float speed = PlayerSpeed * (1 - MaximumSlowdown * static_cast<float>(carried) / GlueCapacity);
    const Vector2 previousPosition = player;
    Vector2 movement;
    if (direction != Vector2{}) {
        target.reset();
        direction = Engine::normalized(direction);
    } else if (target) {
        const Vector2 offset = *target - player;
        if (Engine::length(offset) <= speed * deltaTime) {
            movement = offset;
            target.reset();
        } else {
            direction = Engine::normalized(offset);
        }
    }
    if (direction != Vector2{}) {
        movement = direction * speed * deltaTime;
    }
    player = movePlayer(player, movement, washed < HomeGoal);
    if (movement != Vector2{}) {
        heading = std::atan2(movement.y, movement.x);
    }
    if (player == previousPosition && movement != Vector2{}) {
        target.reset();
    }

    bool inOrange = false;
    bool inBlue = false;
    for (const Puddle& puddle : Puddles) {
        if (Engine::intersects(puddle.shape, Circle{ player, PlayerRadius })) {
            inOrange = inOrange || puddle.sticky;
            inBlue = inBlue || !puddle.sticky;
        }
    }
    if (inBlue) {
        if (carried > 0) {
            for (Trash& item : trash) {
                if (item.state == TrashState::Carried) {
                    item.state = TrashState::Washed;
                }
            }
            lastDelivery = carried;
            // За доставку партии от пяти предметов начисляется дополнительная награда.
            const int reward = carried * DeliveryReward + (carried >= 5 ? carried : 0);
            const int previousMotivation = motivation;
            motivation = std::min(MaximumMotivation, motivation + reward);
            motivationChange = motivation - previousMotivation;
            feedbackTime = 2;
            washed += carried;
            carried = 0;
        }
        coat = Coat::Clean;
        glue = 0;
        cargoAge = 0;
        nextDropTime = GlueLifetime;
    } else if (inOrange && coat != Coat::Sticky) {
        coat = Coat::Sticky;
        glue = GlueCapacity;
        cargoAge = 0;
        nextDropTime = GlueLifetime;
    }

    if (coat == Coat::Sticky && glue > 0 && cargoAge < GlueLifetime) {
        const float pickupRadius = PlayerRadius + static_cast<float>(carried);
        for (Trash& item : trash) {
            if (glue == 0) {
                break;
            }
            if (item.state == TrashState::Ground && item.pickupDelay == 0
                && Engine::intersects(Circle{ player, pickupRadius }, Circle{ item.position, TrashRadius })) {
                item.state = TrashState::Carried;
                item.attachment = Engine::rotated({ PlayerRadius + 9 + 4 * std::sqrt(static_cast<float>(carried)), 0 },
                    static_cast<float>(carried) * GoldenAngle);
                ++carried;
                --glue;
            }
        }
    }

    if (carried > 0) {
        cargoAge += deltaTime;
        while (cargoAge >= nextDropTime && carried > 0) {
            for (Trash& item : trash) {
                if (item.state == TrashState::Carried) {
                    item.state = TrashState::Ground;
                    item.position = Engine::closestPoint({ TrashRadius, TrashRadius,
                        WorldBounds.size.width - 2 * TrashRadius, WorldBounds.size.height - 2 * TrashRadius },
                        player + Engine::rotated(item.attachment, heading));
                    // Упавший предмет остаётся доступен для повторного сбора.
                    if (blocked(item.position, TrashRadius + PlayerRadius, washed < HomeGoal)) {
                        item.position = player;
                    }
                    for (const Puddle& puddle : Puddles) {
                        if (!puddle.sticky && Engine::intersects(puddle.shape, Circle{ item.position, TrashRadius })) {
                            item.position = player;
                        }
                    }
                    item.pickupDelay = DroppedPickupDelay;
                    --carried;
                    break;
                }
            }
            const int previousMotivation = motivation;
            motivation = std::max(0, motivation - DropPenalty);
            motivationChange = motivation - previousMotivation;
            feedbackTime = 2;
            nextDropTime += DropInterval;
            if (motivation == 0) {
                outcome = Outcome::Lost;
                target.reset();
                return;
            }
        }
    }
    if (washed >= HomeGoal && Engine::contains(HomeBounds, player)) {
        outcome = Outcome::Won;
        target.reset();
    }
}
}
