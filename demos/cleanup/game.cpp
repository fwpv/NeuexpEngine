#include "game.h"

#include <algorithm>
#include <cmath>

namespace Cleanup
{
Game::Game()
{
    // Первые предметы расположены рядом с начальной оранжевой лужей.
    trash[0].position = { 1410, 1050 };
    trash[1].position = { 1450, 1100 };
    trash[2].position = { 1370, 1150 };
    for (int index = 3; index < TrashCount; ++index) {
        Vector2 point = {
            160.0F + static_cast<float>((index * 463) % 2680),
            150.0F + static_cast<float>((index * 347) % 1800)
        };
        for (const Puddle& puddle : Puddles) {
            if (Engine::intersects(puddle.shape, Circle{ point, TrashRadius + 20 })) {
                point.x += 190;
            }
        }
        // Предметы доступны до открытия дома.
        if (Engine::intersects(HomeBounds, Circle{ point, TrashRadius + PlayerRadius })) {
            point.y = HomeBounds.position.y - 80;
        }
        trash[static_cast<std::size_t>(index)].position = Engine::closestPoint({ 100, 100, 2800, 1900 }, point);
    }
}

void Game::update(Vector2 direction, float deltaTime)
{
    if (outcome != Outcome::Playing) {
        return;
    }
    feedbackTime = std::max(0.0F, feedbackTime - deltaTime);
    for (Trash& item : trash) {
        item.pickupDelay = std::max(0.0F, item.pickupDelay - deltaTime);
    }

    const float speed = PlayerSpeed * (1 - MaximumSlowdown * static_cast<float>(carried) / GlueCapacity);
    const Vector2 previousPosition = player;
    if (direction != Vector2{}) {
        target.reset();
        direction = Engine::normalized(direction);
    } else if (target) {
        const Vector2 offset = *target - player;
        if (Engine::length(offset) <= speed * deltaTime) {
            player = *target;
            target.reset();
        } else {
            direction = Engine::normalized(offset);
        }
    }
    if (direction != Vector2{}) {
        player += direction * speed * deltaTime;
        heading = std::atan2(direction.y, direction.x);
    }
    player = Engine::closestPoint({ PlayerRadius, PlayerRadius,
        WorldBounds.size.width - 2 * PlayerRadius, WorldBounds.size.height - 2 * PlayerRadius }, player);
    if (washed < HomeGoal && Engine::intersects(HomeBounds, Circle{ player, PlayerRadius })) {
        player = previousPosition;
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
                    // Упавшие предметы остаются доступны до открытия дома и вне моющей лужи.
                    if (washed < HomeGoal && Engine::intersects(HomeBounds, Circle{ item.position, TrashRadius })) {
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
