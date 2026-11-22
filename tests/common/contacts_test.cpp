#include "collision/contacts.h"
#include "collision/collider.h"
#include "collision/query.h"
#include "core/math2d.h"
#include "scene/scene.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool closeTo(float first, float second, float tolerance)
{
    return std::fabs(first - second) <= tolerance;
}

NeuexpEngine::ObjectId addCircle(NeuexpEngine::Scene& scene, NeuexpEngine::Vector2 center, float radius)
{
    using namespace NeuexpEngine;
    auto object = std::make_unique<GameObject>();
    object->transform().position = center;
    object->collider() = Collider{Circle{ { 0, 0 }, radius }};
    return scene.add(std::move(object));
}

NeuexpEngine::ObjectId addBox(NeuexpEngine::Scene& scene, NeuexpEngine::Vector2 center, NeuexpEngine::Size2 size,
    float rotation)
{
    using namespace NeuexpEngine;
    auto object = std::make_unique<GameObject>();
    object->transform().position = center;
    object->transform().rotation = rotation;
    object->collider() = Collider{Box{ { 0, 0 }, size, 0 }};
    return scene.add(std::move(object));
}

int countPhase(const std::vector<NeuexpEngine::ContactEvent>& events, NeuexpEngine::ContactPhase phase)
{
    return static_cast<int>(std::count_if(events.begin(), events.end(),
        [phase](const NeuexpEngine::ContactEvent& event) { return event.phase == phase; }));
}

void testBeginsAndEndsOnce()
{
    using namespace NeuexpEngine;
    Scene scene;
    ContactTracker tracker;
    const ObjectId first = addCircle(scene, { 0, 0 }, 5);
    const ObjectId second = addCircle(scene, { 6, 0 }, 5);

    const std::vector<ContactEvent> begun = tracker.update(scene);
    require(countPhase(begun, ContactPhase::Begin) == 1 && begun.size() == 1, "Contact must begin once.");
    require(!begun.front().contact.trigger, "Plain contact must not be a trigger.");
    require(closeTo(begun.front().contact.manifold.depth, 4, 1e-4F), "Contact depth failed.");
    require(begun.front().contact.first < begun.front().contact.second, "Contact identifiers must be ordered.");
    require((begun.front().contact.first == first && begun.front().contact.second == second),
        "Contact must name both objects.");

    const std::vector<ContactEvent> continued = tracker.update(scene);
    require(continued.size() == 1 && countPhase(continued, ContactPhase::Persist) == 1,
        "Contact must persist without a new begin.");

    scene.find(second)->transform().position = Vector2{20, 0};
    const std::vector<ContactEvent> ended = tracker.update(scene);
    require(ended.size() == 1 && countPhase(ended, ContactPhase::End) == 1, "Contact must end once.");
    require(tracker.update(scene).empty(), "Ended contact must not repeat.");
}

void testManifoldSeparates()
{
    using namespace NeuexpEngine;
    Scene scene;
    ContactTracker tracker;
    addCircle(scene, { 0, 0 }, 5);
    const ObjectId second = addCircle(scene, { 6, 0 }, 5);

    const std::vector<ContactEvent> contact = tracker.update(scene);
    require(contact.size() == 1, "Contact must begin.");
    const Manifold manifold = contact.front().contact.manifold;
    GameObject* moved = scene.find(second);
    require(moved != nullptr, "Second object of the contact is missing.");
    moved->transform().position += manifold.normal * manifold.depth;
    const std::vector<ContactEvent> touching = tracker.update(scene);
    require(touching.size() == 1 && countPhase(touching, ContactPhase::Persist) == 1
        && closeTo(touching.front().contact.manifold.depth, 0, 1e-3F),
        "Shift by the manifold must only make the shapes touch.");
    moved->transform().position += manifold.normal;
    require(countPhase(tracker.update(scene), ContactPhase::End) == 1, "Manifold does not separate the pair.");
}

void testDeletionAndClear()
{
    using namespace NeuexpEngine;
    Scene scene;
    ContactTracker tracker;
    const ObjectId kept = addCircle(scene, { 0, 0 }, 5);
    const ObjectId removed = addCircle(scene, { 4, 0 }, 5);
    tracker.update(scene);

    scene.destroy(removed);
    const std::vector<ContactEvent> afterDestroy = tracker.update(scene);
    require(afterDestroy.size() == 1 && countPhase(afterDestroy, ContactPhase::End) == 1
        && afterDestroy.front().contact.first == kept, "Deleted object must end the contact once.");
    require(tracker.update(scene).empty(), "Contact of a deleted object must not repeat.");

    Scene cleared;
    ContactTracker clearedTracker;
    addCircle(cleared, { 0, 0 }, 5);
    addCircle(cleared, { 4, 0 }, 5);
    clearedTracker.update(cleared);
    cleared.clear();
    const std::vector<ContactEvent> afterClear = clearedTracker.update(cleared);
    require(afterClear.size() == 1 && countPhase(afterClear, ContactPhase::End) == 1,
        "Scene clear must end the contact once.");
}

void testSceneSwitch()
{
    using namespace NeuexpEngine;
    Scene firstScene;
    Scene secondScene;
    ContactTracker tracker;
    // Идентификаторы обеих сцен начинаются с единицы, поэтому пары нельзя продолжать.
    addCircle(firstScene, { 0, 0 }, 5);
    addCircle(firstScene, { 4, 0 }, 5);
    tracker.update(firstScene);
    addCircle(secondScene, { 0, 0 }, 5);
    addCircle(secondScene, { 4, 0 }, 5);
    const std::vector<ContactEvent> switched = tracker.update(secondScene);
    require(countPhase(switched, ContactPhase::End) == 1 && countPhase(switched, ContactPhase::Begin) == 1
        && countPhase(switched, ContactPhase::Persist) == 0,
        "Scene switch must end old contacts and begin new ones.");
}

void testLayersMasksAndDisabled()
{
    using namespace NeuexpEngine;
    Scene scene;
    ContactTracker tracker;
    const ObjectId first = addCircle(scene, { 0, 0 }, 5);
    const ObjectId second = addCircle(scene, { 4, 0 }, 5);
    scene.find(first)->collider().mask = 1U;
    scene.find(second)->collider().layer = 2U;
    require(tracker.update(scene).empty(), "Mask must exclude the pair.");

    scene.find(second)->collider().layer = 1U;
    require(countPhase(tracker.update(scene), ContactPhase::Begin) == 1, "Allowed layer must begin a contact.");

    scene.find(second)->collider().enabled = false;
    require(countPhase(tracker.update(scene), ContactPhase::End) == 1, "Disabled collider must end the contact.");

    scene.find(second)->collider().enabled = true;
    require(countPhase(tracker.update(scene), ContactPhase::Begin) == 1, "Enabled collider must begin the contact.");
}

void testTriggers()
{
    using namespace NeuexpEngine;
    Scene scene;
    ContactTracker tracker;
    const ObjectId zone = addCircle(scene, { 0, 0 }, 8);
    const ObjectId walker = addCircle(scene, { 4, 0 }, 2);
    scene.find(zone)->collider().trigger = true;
    const std::vector<ContactEvent> triggered = tracker.update(scene);
    require(triggered.size() == 1 && triggered.front().contact.trigger, "Trigger contact must be marked.");
    require(scene.find(walker)->transform().position == Vector2{ 4, 0 }, "Trigger must not move the object.");
}

void testTurnedBoxes()
{
    using namespace NeuexpEngine;
    Scene scene;
    ContactTracker tracker;
    addBox(scene, { 0, 0 }, { 10, 10 }, degreesToRadians(45));
    addBox(scene, { 6, 0 }, { 10, 10 }, degreesToRadians(-45));
    require(countPhase(tracker.update(scene), ContactPhase::Begin) == 1, "Turned boxes must touch.");

    Scene parallel;
    ContactTracker parallelTracker;
    addBox(parallel, { 0, 0 }, { 20, 1 }, degreesToRadians(45));
    addBox(parallel, { 0, 5 }, { 20, 1 }, degreesToRadians(45));
    require(parallelTracker.update(parallel).empty(),
        "Separated turned boxes must not touch even with overlapping bounds.");
}

// Поиск пар по x не должен терять контакты: число найденных пар сравнивается
// с полным перебором, посчитанным в самом тесте.
void testPairSweepMatchesFullScan()
{
    using namespace NeuexpEngine;
    constexpr int Columns = 7;
    constexpr int Rows = 6;
    constexpr float Spacing = 34;
    constexpr float Radius = 20;
    Scene scene;
    ContactTracker tracker;
    for (int row = 0; row < Rows; ++row) {
        for (int column = 0; column < Columns; ++column) {
            addCircle(scene, {static_cast<float>(column) * Spacing, static_cast<float>(row) * Spacing}, Radius);
        }
    }

    int expected = 0;
    const int count = Columns * Rows;
    for (int first = 0; first < count; ++first) {
        for (int second = first + 1; second < count; ++second) {
            const float firstX = static_cast<float>(first % Columns) * Spacing;
            const float firstY = static_cast<float>(first / Columns) * Spacing;
            const float secondX = static_cast<float>(second % Columns) * Spacing;
            const float secondY = static_cast<float>(second / Columns) * Spacing;
            if (std::hypot(firstX - secondX, firstY - secondY) <= Radius * 2) {
                ++expected;
            }
        }
    }
    require(expected == 71, "The test layout must have the expected number of touching circles.");

    const std::vector<ContactEvent> events = tracker.update(scene);
    require(countPhase(events, ContactPhase::Begin) == expected && events.size() == static_cast<std::size_t>(expected),
        "Pair search must report every overlapping pair once.");
    const std::vector<ContactEvent> next = tracker.update(scene);
    require(countPhase(next, ContactPhase::Persist) == expected && countPhase(next, ContactPhase::Begin) == 0,
        "Pair search must keep the same pairs without a new begin.");
}

void testAllPairs()
{
    using namespace NeuexpEngine;
    Scene scene;
    ContactTracker tracker;
    // Пять кругов на расстоянии единицы: пересекаются все десять пар.
    for (int index = 0; index < 5; ++index) {
        addCircle(scene, { static_cast<float>(index), 0 }, 3);
    }
    const std::vector<ContactEvent> events = tracker.update(scene);
    require(countPhase(events, ContactPhase::Begin) == 10, "Not all overlapping pairs were reported.");
    require(countPhase(tracker.update(scene), ContactPhase::Persist) == 10, "Persisting pairs were lost.");
    require(scene.findAll().size() == scene.count(), "Full query must match the object count.");
}
}

int main()
{
    try {
        testBeginsAndEndsOnce();
        testManifoldSeparates();
        testDeletionAndClear();
        testSceneSwitch();
        testLayersMasksAndDisabled();
        testTriggers();
        testTurnedBoxes();
        testAllPairs();
        testPairSweepMatchesFullScan();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
