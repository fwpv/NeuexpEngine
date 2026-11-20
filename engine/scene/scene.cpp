#include "scene.h"
#include "graphics/renderer.h"

#include <algorithm>
#include <stdexcept>

namespace
{
// Обход объектов нельзя прерывать освобождением памяти, поэтому clear() внутри update()
// только помечает объекты удалёнными.
class UpdateGuard
{
public:
    explicit UpdateGuard(bool& value)
        : flag(value)
    {
        flag = true;
    }

    ~UpdateGuard()
    {
        flag = false;
    }

    UpdateGuard(const UpdateGuard&) = delete;
    UpdateGuard& operator=(const UpdateGuard&) = delete;

private:
    bool& flag;
};
}

namespace NeuexpEngine
{
Scene::~Scene() = default;

ObjectId Scene::add(std::unique_ptr<GameObject> object)
{
    if (!object) {
        throw std::invalid_argument("Scene::add needs an object.");
    }
    object->objectId = nextObjectId;
    ++nextObjectId;
    objects.push_back(std::move(object));
    return objects.back()->objectId;
}

void Scene::destroy(ObjectId id)
{
    for (const auto& object : objects) {
        if (!object->removalPending && object->objectId == id) {
            object->removalPending = true;
            return;
        }
    }
}

GameObject* Scene::find(ObjectId id)
{
    for (const auto& object : objects) {
        if (!object->removalPending && object->objectId == id) {
            return object.get();
        }
    }
    return nullptr;
}

const GameObject* Scene::find(ObjectId id) const
{
    for (const auto& object : objects) {
        if (!object->removalPending && object->objectId == id) {
            return object.get();
        }
    }
    return nullptr;
}

GameObject* Scene::findByName(std::string_view name)
{
    for (const auto& object : objects) {
        if (!object->removalPending && object->objectName == name) {
            return object.get();
        }
    }
    return nullptr;
}

const GameObject* Scene::findByName(std::string_view name) const
{
    for (const auto& object : objects) {
        if (!object->removalPending && object->objectName == name) {
            return object.get();
        }
    }
    return nullptr;
}

std::vector<GameObject*> Scene::findByGroup(std::string_view group)
{
    std::vector<GameObject*> result;
    for (const auto& object : objects) {
        if (!object->removalPending && object->objectGroup == group) {
            result.push_back(object.get());
        }
    }
    return result;
}

std::vector<const GameObject*> Scene::findByGroup(std::string_view group) const
{
    std::vector<const GameObject*> result;
    for (const auto& object : objects) {
        if (!object->removalPending && object->objectGroup == group) {
            result.push_back(object.get());
        }
    }
    return result;
}

std::size_t Scene::count() const
{
    const auto live = std::count_if(objects.begin(), objects.end(),
        [](const std::unique_ptr<GameObject>& object) { return !object->removalPending; });
    return static_cast<std::size_t>(live);
}

void Scene::update(float deltaTime)
{
    releaseRemoved();
    // Объекты, добавленные во время обновления, начинают обновляться следующим вызовом,
    // поэтому проход ограничен числом объектов на его начало.
    const std::size_t scheduled = objects.size();
    const UpdateGuard guard(updating);
    for (std::size_t index = 0; index < scheduled; ++index) {
        if (!objects[index]->removalPending) {
            objects[index]->update(deltaTime);
        }
    }
    releaseRemoved();
}

void Scene::draw(Renderer2D& renderer) const
{
    for (const auto& object : objects) {
        if (!object->removalPending && object->visible) {
            object->draw(renderer);
        }
    }
}

bool Scene::createView(int width, int height, CameraView& result, CameraError& error) const
{
    return viewCamera.createView(Viewport{0, 0, width, height}, result, error);
}

void Scene::clear()
{
    if (updating) {
        for (const auto& object : objects) {
            object->removalPending = true;
        }
        return;
    }
    objects.clear();
}

void Scene::releaseRemoved()
{
    const auto removed = std::remove_if(objects.begin(), objects.end(),
        [](const std::unique_ptr<GameObject>& object) { return object->removalPending; });
    objects.erase(removed, objects.end());
}
}
