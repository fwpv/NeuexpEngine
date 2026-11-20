#include "scene.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace NeuexpEngine;

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Объект, записывающий вызовы жизненного цикла в общий журнал.
class Recorder : public GameObject
{
public:
    Recorder(std::vector<std::string>& journal, std::string label)
        : journal(journal)
        , label(std::move(label))
    {
    }

    ~Recorder() override
    {
        journal.push_back("destroy " + label);
    }

    void update(float deltaTime) override
    {
        lastDeltaTime = deltaTime;
        journal.push_back("update " + label);
        if (onUpdate) {
            onUpdate();
        }
    }

    std::function<void()> onUpdate;
    float lastDeltaTime = 0;

private:
    std::vector<std::string>& journal;
    std::string label;
};

Recorder& attach(Scene& scene, std::vector<std::string>& journal, const char* label)
{
    auto object = std::make_unique<Recorder>(journal, label);
    object->setName(label);
    Recorder& reference = *object;
    scene.add(std::move(object));
    return reference;
}

void testIdentityAndQueries()
{
    Scene scene;
    auto first = std::make_unique<GameObject>();
    first->setName("ship");
    first->setGroup("friendly");
    GameObject* firstPointer = first.get();
    const ObjectId firstId = scene.add(std::move(first));
    auto second = std::make_unique<GameObject>();
    second->setName("ship");
    second->setGroup("friendly");
    GameObject* secondPointer = second.get();
    const ObjectId secondId = scene.add(std::move(second));
    auto third = std::make_unique<GameObject>();
    third->setName("rock");
    third->setGroup("neutral");
    GameObject* thirdPointer = third.get();
    const ObjectId thirdId = scene.add(std::move(third));

    require(firstId != 0 && secondId != 0 && thirdId != 0, "Scene must assign non-zero identifiers.");
    require(firstId < secondId && secondId < thirdId, "Identifiers must grow in add order.");
    require(firstPointer->id() == firstId && thirdPointer->id() == thirdId, "Object must report its identifier.");
    require(scene.count() == 3, "Scene must count live objects.");
    require(scene.find(firstId) == firstPointer && scene.find(thirdId) == thirdPointer, "Find by identifier failed.");
    require(scene.find(secondId + 100) == nullptr, "Unknown identifier must not be found.");
    require(scene.find(0) == nullptr, "Zero identifier must not be found.");
    require(scene.findByName("ship") == firstPointer, "Find by name must return the oldest object.");
    require(scene.findByName("rock") == thirdPointer, "Find by another name failed.");
    require(scene.findByName("Ship") == nullptr, "Names must compare exactly.");

    const std::vector<GameObject*> friendly = scene.findByGroup("friendly");
    require(friendly.size() == 2 && friendly[0] == firstPointer && friendly[1] == secondPointer,
        "Group query must return objects in add order.");
    require(scene.findByGroup("neutral").size() == 1, "Second group query failed.");
    require(scene.findByGroup("missing").empty(), "Empty group query must be empty.");

    firstPointer->setName("scout");
    firstPointer->setGroup("neutral");
    require(scene.findByName("scout") == firstPointer && scene.findByName("ship") == secondPointer,
        "Renaming must change query results.");
    require(scene.findByGroup("friendly").size() == 1 && scene.findByGroup("neutral").size() == 2,
        "Group change must affect queries.");

    const Scene& view = scene;
    require(view.find(firstId) == firstPointer && view.findByName("scout") == firstPointer, "Const queries failed.");
    require(view.findByGroup("neutral").size() == 2, "Const group query failed.");

    firstPointer->transform().position = Vector2{40, 25};
    const Scene& placement = scene;
    require(placement.find(firstId)->transform().position == Vector2{40, 25}, "Transform access changed value.");
}

void testUpdateOrder()
{
    std::vector<std::string> journal;
    Scene scene;
    Recorder& first = attach(scene, journal, "first");
    attach(scene, journal, "second");
    Recorder& third = attach(scene, journal, "third");

    scene.update(0.25F);
    const std::vector<std::string> expected{"update first", "update second", "update third"};
    require(journal == expected, "Update must follow add order.");
    require(first.lastDeltaTime == 0.25F && third.lastDeltaTime == 0.25F, "Update must pass delta time.");
    journal.clear();

    third.setVisible(false);
    scene.update(0.5F);
    require(journal.size() == 3 && journal[2] == "update third", "Hidden object must keep updating.");
    require(scene.findByName("third") != nullptr && scene.count() == 3, "Hidden object must stay available.");
}

void testDeferredRemoval()
{
    std::vector<std::string> journal;
    Scene scene;
    Recorder& first = attach(scene, journal, "first");
    attach(scene, journal, "second");
    const ObjectId removedId = attach(scene, journal, "third").id();
    bool removalChecked = false;
    first.onUpdate = [&] {
        if (removalChecked) {
            return;
        }
        scene.destroy(removedId);
        require(scene.count() == 2, "Destroyed object must stop counting immediately.");
        require(scene.find(removedId) == nullptr, "Destroyed object must not be found.");
        require(scene.findByName("third") == nullptr, "Destroyed object must not be found by name.");
        require(scene.findByGroup("").size() == 2, "Destroyed object must not appear in group queries.");
        scene.destroy(removedId);
        scene.destroy(removedId + 1000);
        removalChecked = true;
    };

    scene.update(0.1F);
    const std::vector<std::string> expected{"update first", "update second", "destroy third"};
    require(journal == expected, "Destroyed object must be skipped and freed at the end of update.");

    journal.clear();
    scene.update(0.1F);
    const std::vector<std::string> secondPass{"update first", "update second"};
    require(journal == secondPass, "Removed object must not be updated again.");

    scene.destroy(first.id());
    require(scene.find(first.id()) == nullptr && scene.count() == 1, "Removal between frames must hide the object.");
    require(journal == secondPass, "Removal must not free memory before update.");
    journal.clear();
    scene.update(0.1F);
    const std::vector<std::string> removalPass{"destroy first", "update second"};
    require(journal == removalPass, "Object removed between frames must be freed by the next update.");
}

void testCreationDuringUpdate()
{
    std::vector<std::string> journal;
    Scene scene;
    Recorder& first = attach(scene, journal, "first");
    attach(scene, journal, "second");
    ObjectId createdId = 0;
    first.onUpdate = [&] {
        if (createdId != 0) {
            return;
        }
        auto created = std::make_unique<Recorder>(journal, "third");
        createdId = scene.add(std::move(created));
        require(scene.find(createdId) != nullptr, "Object added during update must be found immediately.");
        require(scene.count() == 3, "Object added during update must be counted immediately.");
    };

    scene.update(0.1F);
    const std::vector<std::string> firstPass{"update first", "update second"};
    require(journal == firstPass, "Object added during update must not update in the same pass.");

    journal.clear();
    scene.update(0.1F);
    const std::vector<std::string> secondPass{"update first", "update second", "update third"};
    require(journal == secondPass, "Added object must update from the next pass in add order.");
}

void testClearAndIdentifiers()
{
    std::vector<std::string> journal;
    Scene scene;
    attach(scene, journal, "first");
    attach(scene, journal, "second");
    const ObjectId lastId = attach(scene, journal, "third").id();
    journal.clear();

    scene.clear();
    require(scene.count() == 0, "Clear must remove all objects.");
    const std::vector<std::string> expected{"destroy first", "destroy second", "destroy third"};
    require(journal == expected, "Clear must free all objects immediately.");
    require(scene.find(lastId) == nullptr && scene.findByName("first") == nullptr, "Clear must hide all objects.");

    auto fresh = std::make_unique<GameObject>();
    const ObjectId freshId = scene.add(std::move(fresh));
    require(freshId > lastId, "Identifiers must not be reused after clear.");

    scene.destroy(freshId);
    scene.clear();
    require(scene.count() == 0, "Clear must free objects pending removal.");
}

void testClearDuringUpdate()
{
    std::vector<std::string> journal;
    Scene scene;
    Recorder& first = attach(scene, journal, "first");
    attach(scene, journal, "second");
    attach(scene, journal, "third");
    bool cleared = false;
    first.onUpdate = [&] {
        if (cleared) {
            return;
        }
        scene.clear();
        require(scene.count() == 0, "Clear during update must remove objects from queries.");
        require(scene.find(first.id()) == nullptr, "Clear during update must hide objects by identifier.");
        require(scene.findByName("second") == nullptr, "Clear during update must hide objects by name.");
        cleared = true;
    };

    scene.update(0.1F);
    require(journal.size() == 4 && journal[0] == "update first",
        "The updating object must not be freed during update.");
    std::vector<std::string> freed{journal.begin() + 1, journal.end()};
    std::sort(freed.begin(), freed.end());
    const std::vector<std::string> expected{"destroy first", "destroy second", "destroy third"};
    require(freed == expected, "Clear during update must free every object once.");
    require(scene.count() == 0, "Scene must stay empty after clearing during update.");
}

void testCamera()
{
    Scene scene;
    CameraView view;
    CameraError error = CameraError::InvalidViewport;
    scene.camera().position = Vector2{120, 80};
    scene.camera().referenceSize = Size2{800, 600};
    require(scene.createView(800, 600, view, error) && error == CameraError::None, "Scene view failed.");
    require(view.worldBounds.position == Vector2{-280, -220} && view.worldBounds.size == Size2{800, 600},
        "Scene view must be centered on its camera.");
    require(view.viewport.width == 800 && view.viewport.height == 600, "Scene view must fill the client area.");

    scene.camera().zoom = 2;
    require(scene.createView(800, 600, view, error) && view.worldBounds.size == Size2{400, 300},
        "Scene camera zoom must change the view.");
    require(!scene.createView(0, 600, view, error) && error == CameraError::InvalidViewport,
        "Empty client area must be rejected.");
    scene.camera().zoom = 0;
    require(!scene.createView(800, 600, view, error) && error == CameraError::InvalidCamera,
        "Non-positive zoom must be rejected.");
}

void testDefaultsAndArguments()
{
    Scene scene;
    auto object = std::make_unique<GameObject>();
    require(object->id() == 0, "Object must not have an identifier before adding.");
    require(object->name().empty() && object->group().empty(), "Name and group must start empty.");
    require(object->isVisible(), "Object must start visible.");
    require(object->transform().position == Vector2{} && object->transform().rotation == 0
        && object->transform().scale == Vector2{1, 1}, "Transform must start at identity.");
    require(!object->visual().texture.isValid(), "Visual must start without a texture.");

    bool rejected = false;
    try {
        scene.add(nullptr);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "Scene must reject an empty object.");
    require(scene.count() == 0, "Rejected object must not change the scene.");
}
}

int main()
{
    try {
        testIdentityAndQueries();
        testUpdateOrder();
        testDeferredRemoval();
        testCreationDuringUpdate();
        testClearAndIdentifiers();
        testClearDuringUpdate();
        testCamera();
        testDefaultsAndArguments();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
