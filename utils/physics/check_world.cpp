// Проверка содержания демо physics без окна: страницы показывают взаимодействия
// тел, события в областях и достигнутые стандарты, а страницы независимы и
// воспроизводимы. В сборку демо не входит: запускается вручную после сборки демо.
#include "world.h"

#include <cctype>
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

// Первое число после подписи в тексте зоны; NAN — подпись не найдена.
float numberAfter(const std::string& text, const char* name)
{
    const std::size_t found = text.find(name);
    if (found == std::string::npos) {
        return NAN;
    }
    std::size_t position = found + std::char_traits<char>::length(name);
    while (position < text.size() && text[position] == ' ') {
        ++position;
    }
    char* end = nullptr;
    const float value = std::strtof(text.c_str() + position, &end);
    return end == text.c_str() + position ? NAN : value;
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

void requireZonesHaveText(const PhysicsDemo::CheckBoard& board)
{
    for (const std::unique_ptr<PhysicsDemo::Zone>& zone : board.zones()) {
        require(!zone->title().empty() && !zone->state().empty(),
            "Zone must have a title and an observation.");
        require(!contains(zone->state(), "nan") && !contains(zone->state(), "inf"),
            "Zone state must stay finite: " + zone->title());
    }
}

void stepBoard(PhysicsDemo::CheckBoard& board, int steps)
{
    for (int step = 0; step < steps; ++step) {
        board.update(StepTime);
    }
}

void goToPage(PhysicsDemo::CheckBoard& board, int page)
{
    while (board.pageIndex() != page) {
        board.nextPage();
    }
}
}

int main()
{
    PhysicsDemo::CheckBoard board;
    board.build();
    require(board.pageCount() == 3, "Expected three pages.");
    require(board.pageIndex() == 0, "Board must start on the first page.");
    require(board.zones().size() == 6, "Every page has six zones.");

    // showPage открывает страницу по индексу и не переставляет страницы.
    board.showPage(2);
    require(board.pageIndex() == 2 && board.pageName() == "INDUSTRY STANDARD",
        "showPage must open the requested page.");
    board.showPage(0);
    require(board.pageIndex() == 0 && board.pageName() == "COLLISIONS AND INTERACTIONS",
        "showPage must return to the first page.");

    // Страница 1: столкновения и взаимодействия тел.
    float carried = 0;
    float frictionSlide = 0;
    float slickSlide = 0;
    bool stackFell = false;
    bool stackStable = false;
    for (int step = 0; step < 600; ++step) {
        board.update(StepTime);
        const PhysicsDemo::Zone* stack = findZone(board, "STACK");
        stackFell = stackFell || contains(stack->state(), "FALLING");
        stackStable = stackStable || contains(stack->state(), "STABLE");
        const float shift = numberAfter(findZone(board, "KINEMATIC CARRIER")->state(), "CRATE SHIFT");
        if (!std::isnan(shift)) {
            carried = std::max(carried, std::fabs(shift));
        }
        const float slid = numberAfter(findZone(board, "FRICTION")->counters(), "SLID");
        if (!std::isnan(slid)) {
            frictionSlide = std::max(frictionSlide, slid);
        }
        const float slick = numberAfter(findZone(board, "FRICTIONLESS")->counters(), "SLID");
        if (!std::isnan(slick)) {
            slickSlide = std::max(slickSlide, slick);
        }
    }
    requireZonesHaveText(board);
    require(numberAfter(findZone(board, "ELASTIC BOUNCE")->counters(), "IMPACTS") > 0,
        "Elastic bounce must report impacts.");
    require(contains(findZone(board, "ELASTIC BOUNCE")->state(), "E 0.90"),
        "Elastic bounce must keep its restitution.");
    require(numberAfter(findZone(board, "FRICTION")->counters(), "RUNS") >= 2,
        "Friction zone must repeat its sliding runs.");
    require(numberAfter(findZone(board, "FRICTIONLESS")->counters(), "RUNS") >= 2,
        "Frictionless zone must repeat its sliding runs.");
    require(slickSlide > 200,
        "Frictionless box must slide across the whole floor.");
    require(numberAfter(findZone(board, "FRICTIONLESS")->state(), "VX") == 300,
        "Frictionless box must keep its speed.");
    require(contains(findZone(board, "STACK")->state(), "LEVELS 5"), "Stack must have five levels.");
    require(stackFell, "Stack must fall before it rests.");
    require(stackStable, "Fallen stack must come to rest.");
    require(numberAfter(findZone(board, "STACK")->counters(), "DROPS") >= 1,
        "Resting stack must fall again after a pause.");
    require(frictionSlide < 200, "Friction must stop the box before the floor ends.");
    require(carried > 10, "Kinematic carrier must carry the crate.");
    require(numberAfter(findZone(board, "FORCE AT POINT")->state(), "W") > 0,
        "Force applied off center must spin the body.");
    require(numberAfter(findZone(board, "FORCE AT POINT")->state(), "VY") > 0,
        "Force applied at a point must move the body down.");

    // Страница 2: события и области.
    board.nextPage();
    bool maskExcluded = false;
    bool maskAllowed = false;
    int removed = 0;
    for (int step = 0; step < 600; ++step) {
        board.update(StepTime);
        const std::string maskState = findZone(board, "SENSOR MASK")->state();
        maskExcluded = maskExcluded || contains(maskState, "EXCLUDED");
        maskAllowed = maskAllowed || contains(maskState, "ALLOWED");
        const float count = numberAfter(findZone(board, "SENSOR REMOVES")->state(), "REMOVED");
        if (!std::isnan(count)) {
            removed = std::max(removed, static_cast<int>(count));
        }
    }
    requireZonesHaveText(board);
    require(numberAfter(findZone(board, "SENSOR AREA")->counters(), "BEGIN") > 0
            && numberAfter(findZone(board, "SENSOR AREA")->counters(), "END") > 0,
        "Sensor must report begin and end.");
    require(maskExcluded && maskAllowed, "Sensor mask must toggle pair filtering.");
    require(removed > 0, "Sensor must remove bodies through deferred removal.");
    require(numberAfter(findZone(board, "IMPACT DATA")->counters(), "IMPACTS") > 0
            && numberAfter(findZone(board, "IMPACT DATA")->state(), "IMPULSE") > 0,
        "Impact zone must report impulses.");
    require(contains(findZone(board, "CONTACT POINTS")->state(), "POINTS 2"),
        "Resting box must have two contact points.");
    require(numberAfter(findZone(board, "CONTACT PHASES")->counters(), "BEGIN") > 0
            && numberAfter(findZone(board, "CONTACT PHASES")->counters(), "PERSIST") > 0
            && numberAfter(findZone(board, "CONTACT PHASES")->counters(), "END") > 0,
        "Contact phases must report begin, persist and end.");

    // Страница 3: стандарты индустрии — CCD, точный отскок, высокая стопка, сон.
    board.nextPage();
    float staticGap = 0;
    float dynamicGap = 0;
    float pusherSpeed = 0;
    float reboundGain = -1000;
    int wakeups = 0;
    bool fellAsleep = false;
    bool tallFell = false;
    bool tallStable = false;
    for (int step = 0; step < 1800; ++step) {
        board.update(StepTime);
        fellAsleep = fellAsleep
            || contains(findZone(board, "SLEEPING ISLANDS")->state(), "ASLEEP");
        const PhysicsDemo::Zone* tall = findZone(board, "TALL STACK");
        tallFell = tallFell || contains(tall->state(), "FALLING");
        tallStable = tallStable || contains(tall->state(), "STABLE");
        const PhysicsDemo::Zone* staticCcd = findZone(board, "CCD VS STATIC");
        const float gap = numberAfter(staticCcd->state(), "GAP");
        if (!std::isnan(gap)) {
            staticGap = std::min(staticGap, gap);
        }
        const float movingGap = numberAfter(findZone(board, "CCD VS DYNAMIC")->state(), "GAP");
        if (!std::isnan(movingGap)) {
            dynamicGap = std::min(dynamicGap, movingGap);
        }
        const float targetSpeed = numberAfter(findZone(board, "CCD VS KINEMATIC")->state(), "TARGET VX");
        if (!std::isnan(targetSpeed)) {
            pusherSpeed = std::max(pusherSpeed, targetSpeed);
        }
        const PhysicsDemo::Zone* exact = findZone(board, "EXACT REBOUND");
        const float gain = numberAfter(exact->state(), "REBOUND")
            - numberAfter(exact->state(), "IMPACT");
        if (!std::isnan(gain)) {
            reboundGain = std::max(reboundGain, gain);
        }
        const float launched = numberAfter(findZone(board, "SLEEPING ISLANDS")->counters(), "WAKEUPS");
        if (!std::isnan(launched)) {
            wakeups = std::max(wakeups, static_cast<int>(launched));
        }
    }
    requireZonesHaveText(board);
    require(numberAfter(findZone(board, "CCD VS STATIC")->counters(), "IMPACTS") > 0,
        "Fast body must hit a thin static wall.");
    require(staticGap > -1, "Sampled CCD must stop a fast body before the wall.");
    require(numberAfter(findZone(board, "CCD VS DYNAMIC")->counters(), "IMPACTS") > 0,
        "A fast body must not tunnel through a thin moving wall.");
    require(dynamicGap > -1, "CCD must keep the gap to the moving wall.");
    require(pusherSpeed > 100, "Kinematic body must push a dynamic body.");
    require(numberAfter(findZone(board, "EXACT REBOUND")->counters(), "BOUNCES") > 0,
        "Elastic impact must report rebounds.");
    require(reboundGain <= 20, "Elastic impact must not gain speed.");
    require(contains(findZone(board, "TALL STACK")->state(), "LEVELS 8"),
        "Tall stack must have eight levels.");
    require(tallFell, "Tall stack must fall before it rests.");
    require(tallStable, "Fallen tall stack must come to rest.");
    require(numberAfter(findZone(board, "TALL STACK")->counters(), "DROPS") >= 2,
        "Tall stack must fall again after a pause.");
    require(wakeups > 0, "Sleeping island must wake up on impact.");
    require(fellAsleep, "Sleeping island must fall asleep as a whole.");

    // Страницы не влияют друг на друга: пока страница не обновляется, её зоны стоят.
    PhysicsDemo::CheckBoard isolated;
    isolated.build();
    stepBoard(isolated, 20);
    const std::string frozen = isolated.zones().front()->state();
    isolated.nextPage();
    stepBoard(isolated, 20);
    goToPage(isolated, 0);
    require(isolated.zones().front()->state() == frozen, "Pages must not affect each other.");

    // Перезапуск остаётся на текущей странице и сбрасывает счётчики.
    board.restart();
    require(board.pageIndex() == 2, "Restart must keep the current page.");
    require(numberAfter(findZone(board, "SLEEPING ISLANDS")->counters(), "WAKEUPS") == 0,
        "Restart must reset the page counters.");

    // Одинаковые шаги дают одинаковый результат в двух независимых фабриках.
    PhysicsDemo::CheckBoard first;
    PhysicsDemo::CheckBoard second;
    first.build();
    second.build();
    goToPage(first, 1);
    goToPage(second, 1);
    stepBoard(first, 400);
    stepBoard(second, 400);
    require(first.zones().front()->state() == second.zones().front()->state(),
        "Zone state depends on the world instance.");
    require(first.zones().front()->counters() == second.zones().front()->counters(),
        "Zone counters depend on the world instance.");

    if (failures == 0) {
        std::cout << "CHECKS OK\n";
        return 0;
    }
    std::cout << "CHECKS FAILED: " << failures << '\n';
    return 1;
}
