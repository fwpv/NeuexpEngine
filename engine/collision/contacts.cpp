#include "contacts.h"

#include <algorithm>
#include <cstddef>

namespace NeuexpEngine
{
const std::vector<ContactEvent>& ContactTracker::update(const Scene& scene)
{
    events.clear();
    // Идентификаторы объектов разных сцен совпадают, поэтому смена сцены завершает
    // прежние пары, а не продолжает их.
    if (&scene != currentScene) {
        currentScene = &scene;
        for (const auto& pair : active) {
            events.push_back({ContactPhase::End, pair.second});
        }
        active.clear();
    }
    collect(scene);
    found.clear();
    checkPairs();
    for (const auto& pair : found) {
        const ContactPhase phase = active.find(pair.first) == active.end()
            ? ContactPhase::Begin
            : ContactPhase::Persist;
        events.push_back({phase, pair.second});
    }
    for (const auto& pair : active) {
        if (found.find(pair.first) == found.end()) {
            events.push_back({ContactPhase::End, pair.second});
        }
    }
    active.swap(found);
    return events;
}

void ContactTracker::collect(const Scene& scene)
{
    entries.clear();
    for (const GameObject* object : scene.findAll()) {
        const Collider& collider = object->collider();
        if (!collider.enabled) {
            continue;
        }
        const CollisionShape shape = collider.worldShape(object->transform());
        entries.push_back({object->id(), collider, shape, bounds(shape)});
    }
}

void ContactTracker::checkPairs()
{
    // Полный перебор проверял бы все пары объектов даже в разреженной сцене. Порядок
    // по левой границе позволяет прервать проход, как только проекции на x разошлись.
    order.resize(entries.size());
    for (std::size_t index = 0; index < order.size(); ++index) {
        order[index] = index;
    }
    std::sort(order.begin(), order.end(), [this](std::size_t first, std::size_t second) {
        return entries[first].bounds.position.x < entries[second].bounds.position.x;
    });
    for (std::size_t first = 0; first < order.size(); ++first) {
        const Rect& current = entries[order[first]].bounds;
        const float right = current.position.x + current.size.width;
        for (std::size_t second = first + 1; second < order.size(); ++second) {
            if (entries[order[second]].bounds.position.x > right) {
                break;
            }
            checkPair(entries[order[first]], entries[order[second]]);
        }
    }
}

void ContactTracker::checkPair(const Entry& first, const Entry& second)
{
    if (!intersects(first.bounds, second.bounds) || !canCollide(first.collider, second.collider)) {
        return;
    }
    // Контакт описывается от меньшего идентификатора к большему, поэтому формы
    // передаются в collide в том же порядке, что и в манифолде.
    const bool firstIsSmaller = first.id < second.id;
    Manifold manifold;
    if (!collide(firstIsSmaller ? first.shape : second.shape, firstIsSmaller ? second.shape : first.shape,
            manifold)) {
        return;
    }
    Contact contact;
    contact.first = firstIsSmaller ? first.id : second.id;
    contact.second = firstIsSmaller ? second.id : first.id;
    contact.manifold = manifold;
    contact.trigger = first.collider.trigger || second.collider.trigger;
    found.emplace(std::pair<ObjectId, ObjectId>{contact.first, contact.second}, contact);
}
}
