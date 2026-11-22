// Проверка содержания демо collision без окна: каждая зона должна показать ожидаемое
// обнаружение, а счётчики переходов — расти. В сборку демо не входит, собирается вручную
// после ./scripts/build_demos.sh:
//   g++ -std=c++17 -O2 -Iengine -Idemos/collision utils/collision/check_demo.cpp \
//       demos/collision/world.cpp build/engine/libneuexpengine.a build/libstb_image.a \
//       -lGL -lX11 -lm -lpthread -ldl -o build/collision_checks
#include "graphics/renderer.h"
#include "world.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{
using namespace NeuexpEngine;

int failures = 0;

void require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool containsText(const std::string& text, const char* part)
{
    return text.find(part) != std::string::npos;
}

// Значение из подписи вида "DISTANCE  12.4": число после последнего пробела.
float trailingNumber(const std::string& text)
{
    const std::size_t space = text.find_last_of(' ');
    if (space == std::string::npos) {
        return 0;
    }
    try {
        return std::stof(text.substr(space + 1));
    } catch (const std::exception&) {
        return 0;
    }
}

// Значение счётчика из подписи вида "BEGIN 3   END 2".
int counterValue(const std::string& counters, const char* name)
{
    const std::size_t found = counters.find(name);
    if (found == std::string::npos) {
        return 0;
    }
    try {
        return std::stoi(counters.substr(found + std::strlen(name)));
    } catch (const std::exception&) {
        return 0;
    }
}

// Наблюдение за одной зоной на нескольких кадрах.
struct Watcher
{
    bool sawContact = false;
    bool sawClear = false;
    bool sawHit = false;
    bool sawMiss = false;
    bool sawInside = false;
    bool sawOutside = false;
    bool countersGrew = false;
    float minimumValue = 1e9F;
    float maximumValue = -1e9F;
};

Watcher watch(const CollisionDemo::Zone& zone)
{
    Watcher watcher;
    const std::string state = zone.state();
    const std::string counters = zone.counters();
    watcher.sawContact = zone.active() || containsText(state, "CONTACT");
    watcher.sawClear = containsText(state, "NO CONTACT") || containsText(state, "CLEAR")
        || containsText(state, "EXCLUDED");
    watcher.sawHit = containsText(state, "HIT");
    watcher.sawMiss = containsText(state, "MISS");
    watcher.sawInside = containsText(state, "INSIDE");
    watcher.sawOutside = containsText(state, "OUTSIDE");
    watcher.countersGrew = !counters.empty() && counters != "BEGIN 0   END 0";
    if (containsText(state, "DISTANCE")) {
        const float value = trailingNumber(state);
        watcher.minimumValue = value;
        watcher.maximumValue = value;
    }
    return watcher;
}

void merge(Watcher& total, const Watcher& frame)
{
    total.sawContact = total.sawContact || frame.sawContact;
    total.sawClear = total.sawClear || frame.sawClear;
    total.sawHit = total.sawHit || frame.sawHit;
    total.sawMiss = total.sawMiss || frame.sawMiss;
    total.sawInside = total.sawInside || frame.sawInside;
    total.sawOutside = total.sawOutside || frame.sawOutside;
    total.countersGrew = total.countersGrew || frame.countersGrew;
    total.minimumValue = std::min(total.minimumValue, frame.minimumValue);
    total.maximumValue = std::max(total.maximumValue, frame.maximumValue);
}
}

// Подписи зоны должны помещаться в её ширину: заголовок крупнее подписей состояния.
void requireLabelsFit(const CollisionDemo::CheckBoard& board)
{
    NeuexpEngine::Renderer2D measure;
    const float inner = CollisionDemo::CellWidth - 24;
    for (const std::unique_ptr<CollisionDemo::Zone>& zone : board.zones()) {
        require(measure.measureDebugText(zone->title(), 2).width <= inner,
            "title does not fit the zone: " + zone->title());
        require(measure.measureDebugText(zone->state(), 1).width <= inner,
            "state line does not fit the zone: " + zone->state());
        if (!zone->counters().empty()) {
            require(measure.measureDebugText(zone->counters(), 1).width <= inner,
                "counters do not fit the zone: " + zone->counters());
        }
    }
}

int main()
{
    CollisionDemo::CheckBoard board;
    require(board.pageCount() == 3, "demo must have three pages");
    require(board.zones().size() == 6, "pair page must contain six zones");
    requireLabelsFit(board);

    // Страница пар: за полный проход каждая зона успевает показать и контакт, и его
    // отсутствие, а счётчики переходов растут.
    std::vector<Watcher> pairWatch(board.zones().size());
    for (int frame = 0; frame < 480; ++frame) {
        board.update(1.0F / 60.0F);
        for (std::size_t index = 0; index < board.zones().size(); ++index) {
            merge(pairWatch[index], watch(*board.zones()[index]));
        }
    }
    for (std::size_t index = 0; index < board.zones().size(); ++index) {
        const std::string& title = board.zones()[index]->title();
        require(pairWatch[index].sawContact, "zone " + title + " never reported a contact");
        require(pairWatch[index].sawClear, "zone " + title + " never reported a separated pair");
        require(pairWatch[index].countersGrew, "zone " + title + " counted no contact transitions");
        std::cout << "pair zone " << title << "  state: " << board.zones()[index]->state() << '\n';
    }

    // Вторая страница: триггер, маска, расстояние, лучи и точка.
    board.nextPage();
    require(board.pageIndex() == 1, "page switch failed");
    require(board.zones().size() == 6, "query page must contain six zones");
    requireLabelsFit(board);
    std::vector<Watcher> queryWatch(board.zones().size());
    for (int frame = 0; frame < 300; ++frame) {
        board.update(1.0F / 60.0F);
        for (std::size_t index = 0; index < board.zones().size(); ++index) {
            merge(queryWatch[index], watch(*board.zones()[index]));
        }
    }
    for (std::size_t index = 0; index < board.zones().size(); ++index) {
        std::cout << "query zone " << board.zones()[index]->title() << "  state: "
                  << board.zones()[index]->state() << "   " << board.zones()[index]->counters() << '\n';
    }

    require(queryWatch[0].sawContact && queryWatch[0].sawClear, "trigger zone must report contact and clear");
    require(queryWatch[1].sawContact, "mask zone must report a contact while the mask allows it");
    require(queryWatch[2].maximumValue > queryWatch[2].minimumValue, "distance zone value must change");
    require(queryWatch[2].minimumValue > 0, "distance zone must never overlap");
    require(queryWatch[3].sawHit && queryWatch[3].sawMiss, "ray zone over a circle must hit and miss");
    require(queryWatch[4].sawHit && queryWatch[4].sawMiss, "ray zone over a box must hit and miss");
    require(queryWatch[5].sawInside && queryWatch[5].sawOutside, "point zone must report inside and outside");

    // Переключатели меняют правило пары и отражаются в подписи.
    board.toggleTrigger();
    require(containsText(board.zones()[0]->state(), "TRIGGER OFF"), "trigger toggle failed");
    board.toggleTrigger();
    require(containsText(board.zones()[0]->state(), "TRIGGER ON"), "trigger toggle back failed");
    board.toggleMask();
    require(containsText(board.zones()[1]->state(), "MASK SLABS ONLY"), "mask toggle failed");
    board.update(1.0F / 60.0F);
    require(containsText(board.zones()[1]->state(), "EXCLUDED"), "mask did not exclude the pair");
    board.toggleMask();
    board.update(1.0F / 60.0F);
    require(containsText(board.zones()[1]->state(), "CONTACT ALLOWED"), "mask did not allow the pair again");

    // Третья страница: жизненный цикл контакта при удалении, остановке и очистке.
    board.nextPage();
    require(board.pageIndex() == 2 && board.zones().size() == 3, "lifecycle page must contain three zones");
    requireLabelsFit(board);
    struct LifecycleWatch
    {
        bool sawContact = false;
        bool sawRemoved = false;
        bool sawStopped = false;
        bool sawEmpty = false;
        int maximumBegun = 0;
        int maximumEnded = 0;
    };
    std::vector<LifecycleWatch> lifecycleWatch(board.zones().size());
    for (int frame = 0; frame < 720; ++frame) {
        board.update(1.0F / 60.0F);
        for (std::size_t index = 0; index < board.zones().size(); ++index) {
            const std::string state = board.zones()[index]->state();
            const std::string counters = board.zones()[index]->counters();
            LifecycleWatch& watcher = lifecycleWatch[index];
            watcher.sawContact = watcher.sawContact || containsText(state, "CONTACT");
            watcher.sawRemoved = watcher.sawRemoved || containsText(state, "REMOVED");
            watcher.sawStopped = watcher.sawStopped || containsText(state, "STOPPED");
            watcher.sawEmpty = watcher.sawEmpty || containsText(state, "EMPTY");
            watcher.maximumBegun = std::max(watcher.maximumBegun, counterValue(counters, "BEGIN"));
            watcher.maximumEnded = std::max(watcher.maximumEnded, counterValue(counters, "END"));
        }
    }
    for (std::size_t index = 0; index < board.zones().size(); ++index) {
        std::cout << "lifecycle zone " << board.zones()[index]->title() << "  state: "
                  << board.zones()[index]->state() << "   " << board.zones()[index]->counters() << '\n';
    }
    require(lifecycleWatch[0].sawContact && lifecycleWatch[0].sawRemoved,
        "delete zone must show a contact and then its removal");
    require(lifecycleWatch[0].maximumBegun >= 2 && lifecycleWatch[0].maximumEnded >= 1,
        "delete zone must begin and end contacts repeatedly");
    require(lifecycleWatch[0].maximumBegun - lifecycleWatch[0].maximumEnded <= 1,
        "delete zone must not end a contact twice");
    require(lifecycleWatch[1].sawContact && lifecycleWatch[1].sawStopped,
        "stop zone must show a held contact and a stopped scene");
    require(lifecycleWatch[1].maximumBegun == 1 && lifecycleWatch[1].maximumEnded == 0,
        "stopping the scene must not repeat a begin");
    require(lifecycleWatch[2].sawContact && lifecycleWatch[2].sawEmpty,
        "clear zone must show a contact and an empty scene");
    require(lifecycleWatch[2].maximumBegun >= 2 && lifecycleWatch[2].maximumEnded >= 1,
        "clear zone must begin and end contacts repeatedly");

    // Перезапуск возвращает зоны в исходное состояние.
    board.restart();
    require(board.pageIndex() == 0, "restart must return to the first page");
    require(containsText(board.zones()[0]->counters(), "BEGIN 0"), "restart must reset counters");

    if (failures == 0) {
        std::cout << "CHECKS OK\n";
        return 0;
    }
    std::cout << "CHECKS FAILED: " << failures << '\n';
    return 1;
}
