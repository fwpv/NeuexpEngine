// Проверка содержания демо physics без окна: мир остаётся устойчивым, состояние тел
// не уходит в NaN, сенсоры считают переходы, яма удаляет тела, одинаковые шаги
// воспроизводимы. В сборку демо не входит, собирается вручную после ./scripts/build_demos.sh:
//   g++ -std=c++17 -O2 -Iengine -Idemos/physics utils/physics/check_world.cpp \
//       demos/physics/world.cpp build/engine/libneuexpengine.a build/libstb_image.a \
//       -lGL -lX11 -lm -lpthread -ldl -o build/physics_checks
#include "world.h"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

namespace
{
using namespace NeuexpEngine;

constexpr float StepTime = 1.0F / 60;

int failures = 0;

void require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void requireFinite(const PhysicsDemo::World& world)
{
    for (const RigidBody* body : world.physics().bodies()) {
        require(std::isfinite(body->position.x) && std::isfinite(body->position.y)
                && std::isfinite(body->velocity.x) && std::isfinite(body->velocity.y)
                && std::isfinite(body->angle) && std::isfinite(body->angularVelocity),
            "Body state must stay finite.");
    }
}
}

int main()
{
    PhysicsDemo::World world;
    world.recreate();
    // Мир выдерживает долгую симуляцию: тела не проваливаются, сенсоры работают,
    // яма удаляет тела, население мира ограничено.
    for (int step = 0; step < 1800; ++step) {
        world.step(StepTime);
        if (step % 120 == 0) {
            requireFinite(world);
        }
    }
    requireFinite(world);
    const PhysicsDemo::Statistics& stats = world.statistics();
    std::cout << "bodies " << world.bodyCount() << "   gate inside " << stats.gateInside
              << "   begin " << stats.gateBegins << "   end " << stats.gateEnds
              << "   pit " << stats.pitRemoved << "   impacts " << stats.impacts << '\n';
    require(stats.gateBegins > 0 && stats.gateEnds > 0, "Gate sensor must report begin and end.");
    require(stats.gateInside >= 0, "Gate inside count must not go negative.");
    require(stats.pitRemoved > 0, "Pit sensor must remove bodies.");
    require(stats.impacts > 0 && stats.lastImpactImpulse > 0, "Contacts must report impulses.");
    require(world.bodyCount() > 0 && world.bodyCount() < 40, "Body population must stay bounded.");

    // Одинаковые шаги дают одинаковый результат независимо от разбиения по кадрам.
    PhysicsDemo::World dense;
    PhysicsDemo::World sparse;
    dense.recreate();
    sparse.recreate();
    for (int step = 0; step < 300; ++step) {
        dense.step(StepTime);
    }
    for (int frame = 0; frame < 75; ++frame) {
        for (int step = 0; step < 4; ++step) {
            sparse.step(StepTime);
        }
    }
    const std::vector<const RigidBody*> first = dense.physics().bodies();
    const std::vector<const RigidBody*> second = sparse.physics().bodies();
    require(first.size() == second.size(), "Equal steps must keep the same bodies.");
    int differences = 0;
    for (std::size_t index = 0; index < first.size() && index < second.size(); ++index) {
        if (std::fabs(first[index]->position.x - second[index]->position.x) > 0.001F
            || std::fabs(first[index]->position.y - second[index]->position.y) > 0.001F) {
            ++differences;
        }
    }
    require(differences == 0, "Fixed step results depend on frame grouping.");

    // Перезапуск возвращает счётчики и население мира к исходному.
    PhysicsDemo::World fresh;
    fresh.recreate();
    world.recreate();
    require(world.statistics().gateBegins == 0 && world.statistics().pitRemoved == 0,
        "Restart must reset the counters.");
    require(world.bodyCount() == fresh.bodyCount(), "Restart must restore the initial bodies.");

    // Управление миром: новый шар, сила ветра, импульс в точке и притяжение курсором.
    PhysicsDemo::World controls;
    controls.recreate();
    const std::size_t beforeSpawn = controls.bodyCount();
    controls.spawnBall();
    require(controls.bodyCount() == beforeSpawn + 1, "Spawning must add one body.");
    controls.recreate();
    require(!controls.impulseAt({550, 380}), "Impulse must not hit empty space.");
    const auto ballAt = [&controls](float x, float y) -> const RigidBody* {
        for (const RigidBody* body : controls.physics().bodies()) {
            if (body->type == BodyType::Dynamic && body->worldShape().kind() == CollisionShape::Kind::Circle
                && std::fabs(body->position.x - x) < 2 && std::fabs(body->position.y - y) < 2) {
                return body;
            }
        }
        return nullptr;
    };
    const RigidBody* target = ballAt(470, 70);
    require(target != nullptr, "Expected ball is missing.");
    require(controls.impulseAt({470, 70}), "Impulse must hit a dynamic body.");
    require(target->velocity.y < -300, "Impulse must throw the body up.");

    controls.recreate();
    controls.setCaret({550, 650});
    controls.setAttract(true);
    controls.step(StepTime);
    require(ballAt(470, 71)->velocity.y > 30, "Attraction must pull the body to the caret.");
    controls.setAttract(false);

    controls.recreate();
    require(!controls.isWindActive(), "Wind must start disabled.");
    controls.toggleWind();
    require(controls.isWindActive(), "Wind toggle failed.");
    controls.step(StepTime);
    require(ballAt(470, 71)->velocity.x > 10, "Wind must push bodies sideways.");

    if (failures == 0) {
        std::cout << "CHECKS OK\n";
        return 0;
    }
    std::cout << "CHECKS FAILED: " << failures << '\n';
    return 1;
}
