#include "game.h"

#include <algorithm>
#include <cmath>

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

Rules::Rules()
    : playerRadius(18)
    , trashRadius(8)
    , homeGoal(40)
    , glueCapacity(10)
    , maximumMotivation(100)
    , passiveMotivationFloor(10)
    , motivationDecayInterval(2)
    , dropPenalty(8)
    , deliveryReward(4)
    , glueLifetime(15)
    , glueWarningTime(10)
{
}

namespace
{
bool blocked(const World& world, Vector2 point, float radius, bool homeLocked)
{
    const Circle body = {point, radius};
    if (homeLocked && Engine::intersects(world.homeBounds, body)) {
        return true;
    }
    return std::any_of(world.obstacles.begin(), world.obstacles.end(), [&](const Rect& obstacle) {
        return Engine::intersects(obstacle, body);
    });
}

bool canPlaceTrash(const World& world, const Rules& rules, Vector2 point)
{
    const float clearance = rules.trashRadius + rules.playerRadius;
    return !blocked(world, point, clearance, true)
        && std::none_of(world.puddles.begin(), world.puddles.end(), [&](const Puddle& puddle) {
            return Engine::intersects(puddle.shape, Circle{point, clearance});
        });
}

Vector2 movePlayer(const World& world, const Rules& rules, Vector2 position, Vector2 movement, bool homeLocked)
{
    const Rect playerBounds = {rules.playerRadius, rules.playerRadius,
        world.bounds.size.width - 2 * rules.playerRadius, world.bounds.size.height - 2 * rules.playerRadius};
    // Короткие шаги исключают проход сквозь препятствие; раздельные оси дают скольжение вдоль стен.
    const int steps = std::max(1, static_cast<int>(std::ceil(Engine::length(movement) / (rules.playerRadius / 4))));
    const Vector2 step = movement / static_cast<float>(steps);
    for (int index = 0; index < steps; ++index) {
        Vector2 next = Engine::closestPoint(playerBounds, position + Vector2{step.x, 0});
        if (!blocked(world, next, rules.playerRadius, homeLocked)) {
            position = next;
        }
        next = Engine::closestPoint(playerBounds, position + Vector2{0, step.y});
        if (!blocked(world, next, rules.playerRadius, homeLocked)) {
            position = next;
        }
    }
    return position;
}
}

Trash::Trash()
    : state(TrashState::Ground)
    , pickupDelay(0)
{
}

Game::Game()
    : player(world.bounds.center())
    , heading(0)
    , coat(Coat::Dry)
    , carried(0)
    , washed(0)
    , glue(0)
    , motivation(60)
    , motivationDecayTime(0)
    , cargoAge(0)
    , nextDropTime(rules.glueLifetime)
    , feedbackTime(0)
    , motivationChange(0)
    , lastDelivery(0)
    , outcome(Outcome::Playing)
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

void Game::update(Vector2 direction, float deltaTime)
{
    if (outcome != Outcome::Playing) {
        return;
    }
    feedbackTime = std::max(0.0F, feedbackTime - deltaTime);
    motivationDecayTime += deltaTime;
    while (motivation > rules.passiveMotivationFloor && motivationDecayTime >= rules.motivationDecayInterval) {
        --motivation;
        motivationDecayTime -= rules.motivationDecayInterval;
    }
    if (motivation <= rules.passiveMotivationFloor) {
        // Потеря груза может опустить мотивацию ниже порога; время её не восстанавливает.
        motivationDecayTime = 0;
    }
    for (Trash& item : trash) {
        item.pickupDelay = std::max(0.0F, item.pickupDelay - deltaTime);
    }

    const float speed = 260.0f * (1 - 0.4F * static_cast<float>(carried) / static_cast<float>(rules.glueCapacity));
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
    player = movePlayer(world, rules, player, movement, washed < rules.homeGoal);
    if (movement != Vector2{}) {
        heading = std::atan2(movement.y, movement.x);
    }
    if (player == previousPosition && movement != Vector2{}) {
        target.reset();
    }

    bool inOrange = false;
    bool inBlue = false;
    for (const Puddle& puddle : world.puddles) {
        if (Engine::intersects(puddle.shape, Circle{player, rules.playerRadius})) {
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
            const int reward = carried * rules.deliveryReward + (carried >= 5 ? carried : 0);
            const int previousMotivation = motivation;
            motivation = std::min(rules.maximumMotivation, motivation + reward);
            motivationChange = motivation - previousMotivation;
            feedbackTime = 2;
            washed += carried;
            carried = 0;
        }
        coat = Coat::Clean;
        glue = 0;
        cargoAge = 0;
        nextDropTime = rules.glueLifetime;
    } else if (inOrange && coat != Coat::Sticky) {
        coat = Coat::Sticky;
        glue = rules.glueCapacity;
        cargoAge = 0;
        nextDropTime = rules.glueLifetime;
    }

    if (coat == Coat::Sticky && glue > 0 && cargoAge < rules.glueLifetime) {
        const float pickupRadius = rules.playerRadius + static_cast<float>(carried);
        for (Trash& item : trash) {
            if (glue == 0) {
                break;
            }
            if (item.state == TrashState::Ground && item.pickupDelay == 0
                && Engine::intersects(Circle{player, pickupRadius}, Circle{item.position, rules.trashRadius})) {
                item.state = TrashState::Carried;
                item.attachment = Engine::rotated(Vector2{rules.playerRadius + 9.0f + 4.0f * std::sqrt(static_cast<float>(carried)), 0},
                    static_cast<float>(carried) * 2.39996323F);
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
                    item.position = Engine::closestPoint(Rect{rules.trashRadius, rules.trashRadius,
                        world.bounds.size.width - 2 * rules.trashRadius, world.bounds.size.height - 2 * rules.trashRadius},
                        player + Engine::rotated(item.attachment, heading));
                    // Упавший предмет остаётся доступен для повторного сбора.
                    if (blocked(world, item.position, rules.trashRadius + rules.playerRadius, washed < rules.homeGoal)) {
                        item.position = player;
                    }
                    for (const Puddle& puddle : world.puddles) {
                        if (!puddle.sticky && Engine::intersects(puddle.shape, Circle{item.position, rules.trashRadius})) {
                            item.position = player;
                        }
                    }
                    item.pickupDelay = 4;
                    --carried;
                    break;
                }
            }
            const int previousMotivation = motivation;
            motivation = std::max(0, motivation - rules.dropPenalty);
            motivationChange = motivation - previousMotivation;
            feedbackTime = 2;
            nextDropTime += 3.0f;
            if (motivation == 0) {
                outcome = Outcome::Lost;
                target.reset();
                return;
            }
        }
    }
    if (washed >= rules.homeGoal && Engine::contains(world.homeBounds, player)) {
        outcome = Outcome::Won;
        target.reset();
    }
}
}
