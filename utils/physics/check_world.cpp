// Проверка содержания демо physics без окна: каждая зона показывает ожидаемое
// поведение, страницы независимы, площадка считает события и воспроизводима.
// В сборку демо не входит, собирается вручную после ./scripts/build_demos.sh:
//   g++ -std=c++17 -O2 -Iengine -Idemos/physics utils/physics/check_world.cpp \
//       demos/physics/world.cpp build/engine/libneuexpengine.a build/libstb_image.a \
//       -lGL -lX11 -lm -lpthread -ldl -o build/physics_checks
#include "world.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>

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

bool contains(const std::string& text, const char* part)
{
    return text.find(part) != std::string::npos;
}

// Целое число после подписи в тексте зоны; -1 — подпись не найдена.
int valueAfter(const std::string& text, const char* name)
{
    const std::size_t found = text.find(name);
    if (found == std::string::npos) {
        return -1;
    }
    std::size_t position = found + std::char_traits<char>::length(name);
    while (position < text.size() && text[position] == ' ') {
        ++position;
    }
    bool negative = false;
    if (position < text.size() && text[position] == '-') {
        negative = true;
        ++position;
    }
    int value = 0;
    bool digits = false;
    while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
        value = value * 10 + (text[position] - '0');
        ++position;
        digits = true;
    }
    return digits ? (negative ? -value : value) : -1;
}

const PhysicsDemo::Zone* findZone(const PhysicsDemo::CheckBoard& board, const char* title)
{
    for (const std::unique_ptr<PhysicsDemo::Zone>& zone : board.zones()) {
        if (zone->title() == title) {
            return zone.get();
        }
    }
    return nullptr;
}

void stepBoard(PhysicsDemo::CheckBoard& board, int steps)
{
    for (int step = 0; step < steps; ++step) {
        board.update(StepTime);
    }
}

void requireZonesHaveText(PhysicsDemo::CheckBoard& board)
{
    for (const std::unique_ptr<PhysicsDemo::Zone>& zone : board.zones()) {
        require(!zone->title().empty() && !zone->state().empty(),
            "Zone must have a title and an observation.");
        require(!contains(zone->state(), "nan") && !contains(zone->state(), "inf"),
            "Zone state must stay finite: " + zone->title());
    }
}
}

int main()
{
    PhysicsDemo::CheckBoard board;
    board.build();
    require(board.pageCount() == 5, "Expected five pages.");
    require(board.pageIndex() == 0, "Board must start on the first page.");
    require(board.zones().size() == 6, "Every small page has six zones.");

    // Страница 1: падение, отскок, затухание, снаряд.
    stepBoard(board, 30);
    requireZonesHaveText(board);
    const PhysicsDemo::Zone* freeFall = findZone(board, "FREE FALL");
    require(freeFall != nullptr && contains(freeFall->state(), "VY"), "Free fall must report speed.");
    require(valueAfter(findZone(board, "LINEAR DAMPING")->state(), "LIMIT") == 360,
        "Linear damping must show terminal velocity.");
    const PhysicsDemo::Zone* bouncy = findZone(board, "RESTITUTION 0.9");
    const PhysicsDemo::Zone* dull = findZone(board, "RESTITUTION 0.1");
    require(bouncy != nullptr && dull != nullptr, "Restitution zones are missing.");
    require(valueAfter(findZone(board, "PROJECTILE")->state(), "VX") == 160,
        "Projectile must keep horizontal speed.");
    stepBoard(board, 300);
    requireZonesHaveText(board);
    require(valueAfter(bouncy->counters(), "IMPACTS") > valueAfter(dull->counters(), "IMPACTS"),
        "Bouncy ball must hit more often than the dull one.");
    require(valueAfter(findZone(board, "ANGULAR DAMPING")->state(), "W") <= 8
            && valueAfter(findZone(board, "ANGULAR DAMPING")->state(), "W") >= 0,
        "Angular damping must stay within the initial speed.");
    board.nextPage();

    // Страница 2: силы, импульсы, момент и их отсутствие у статики.
    stepBoard(board, 120);
    requireZonesHaveText(board);
    require(valueAfter(findZone(board, "CONSTANT FORCE")->state(), "VX") > 0,
        "Constant force must accelerate the body.");
    require(valueAfter(findZone(board, "IMPULSE")->counters(), "IMPULSES") >= 1,
        "Impulse zone must apply kicks.");
    require(valueAfter(findZone(board, "TORQUE")->state(), "W") > 0,
        "Torque must spin the body.");
    require(contains(findZone(board, "STATIC / KINEMATIC")->counters(), "IGNORED"),
        "Static and kinematic bodies must ignore forces.");
    require(valueAfter(findZone(board, "STATIC / KINEMATIC")->state(), "STATIC SHIFT") == 0,
        "Static body must not move.");
    board.nextPage();

    // Страница 3: масса, трение и стопка.
    stepBoard(board, 300);
    requireZonesHaveText(board);
    require(contains(findZone(board, "DENSITY TO MASS")->state(), "MASS 800 / 3200"),
        "Density must define mass.");
    require(valueAfter(findZone(board, "FRICTION")->counters(), "RUNS") >= 1,
        "Friction zone must show sliding runs.");
    require(valueAfter(findZone(board, "FRICTIONLESS")->state(), "VX") == 300,
        "Frictionless box must keep its speed.");
    require(contains(findZone(board, "STACK")->state(), "STABLE"),
        "Stack must stay stable.");
    require(valueAfter(findZone(board, "FORCE VS FRICTION")->counters(), "LIMIT") > 0,
        "Friction limit must be reported.");
    board.nextPage();

    // Страница 4: сенсоры, контакты и кинематическое движение.
    bool maskExcluded = false;
    bool maskAllowed = false;
    int removed = 0;
    int carried = 0;
    for (int step = 0; step < 360; ++step) {
        board.update(StepTime);
        const std::string maskState = findZone(board, "SENSOR MASK")->state();
        maskExcluded = maskExcluded || contains(maskState, "EXCLUDED");
        maskAllowed = maskAllowed || contains(maskState, "ALLOWED");
        removed = std::max(removed, valueAfter(findZone(board, "SENSOR REMOVES")->state(), "REMOVED"));
        const int shift = valueAfter(findZone(board, "KINEMATIC CARRIER")->state(), "CRATE SHIFT");
        carried = std::max(carried, std::abs(shift));
    }
    requireZonesHaveText(board);
    const PhysicsDemo::Zone* sensorPass = findZone(board, "SENSOR PASS");
    require(valueAfter(sensorPass->counters(), "BEGIN") > 0
            && valueAfter(sensorPass->counters(), "END") > 0,
        "Sensor must report begin and end.");
    require(valueAfter(sensorPass->state(), "SPEED") == 150,
        "Sensor must not slow the body down.");
    require(maskExcluded && maskAllowed, "Sensor mask must toggle pair filtering.");
    require(removed > 0, "Sensor must remove bodies through deferred removal.");
    require(valueAfter(findZone(board, "IMPACT IMPULSE")->counters(), "IMPACTS") > 0,
        "Impact zone must report contacts.");
    require(contains(findZone(board, "CONTACT POINTS")->state(), "POINTS 2"),
        "Resting box must have two contact points.");
    require(carried > 10, "Kinematic platform must carry the crate.");
    board.nextPage();

    // Страница 5: площадка.
    require(board.isPlaygroundPage(), "Last page must be the playground.");
    stepBoard(board, 1200);
    requireZonesHaveText(board);
    const PhysicsDemo::PlaygroundStatistics* stats = board.playgroundStatistics();
    require(stats != nullptr, "Playground statistics are missing.");
    require(stats->gateBegins > 0 && stats->gateEnds > 0, "Gate sensor must count transitions.");
    require(stats->gateInside >= 0, "Gate inside count must not be negative.");
    require(stats->pitRemoved > 0, "Pit sensor must remove bodies.");
    require(stats->impacts > 0 && stats->lastImpactImpulse > 0, "Playground must report impacts.");

    // Страницы не влияют друг на друга: пока страница не обновляется, её зоны стоят.
    PhysicsDemo::CheckBoard isolated;
    isolated.build();
    stepBoard(isolated, 20);
    const std::string frozen = isolated.zones().front()->state();
    isolated.nextPage();
    stepBoard(isolated, 20);
    while (isolated.pageIndex() != 0) {
        isolated.nextPage();
    }
    require(isolated.zones().front()->state() == frozen, "Pages must not affect each other.");

    // Перезапуск возвращает счётчики площадки и остаётся на её странице.
    board.restart();
    require(board.isPlaygroundPage(), "Restart must keep the current page.");
    require(board.playgroundStatistics() != nullptr
            && board.playgroundStatistics()->gateBegins == 0
            && board.playgroundStatistics()->pitRemoved == 0,
        "Restart must reset the playground counters.");

    // Одинаковые шаги дают одинаковый результат в двух независимых мирах.
    PhysicsDemo::CheckBoard first;
    PhysicsDemo::CheckBoard second;
    first.build();
    second.build();
    while (!first.isPlaygroundPage()) {
        first.nextPage();
    }
    while (!second.isPlaygroundPage()) {
        second.nextPage();
    }
    stepBoard(first, 400);
    stepBoard(second, 400);
    require(first.zones().front()->state() == second.zones().front()->state(),
        "Playground state depends on the world instance.");
    require(first.zones().front()->counters() == second.zones().front()->counters(),
        "Playground counters depend on the world instance.");

    if (failures == 0) {
        std::cout << "CHECKS OK\n";
        return 0;
    }
    std::cout << "CHECKS FAILED: " << failures << '\n';
    return 1;
}
