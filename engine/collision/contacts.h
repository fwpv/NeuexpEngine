#pragma once

#include "collider.h"
#include "query.h"
#include "scene/scene.h"

#include <map>
#include <utility>
#include <vector>

namespace NeuexpEngine
{
// Фаза контакта пары объектов.
enum class ContactPhase
{
    // Пара появилась в этом вызове.
    Begin,
    // Пара была в прошлом вызове и остаётся; данные контакта относятся к этому вызову.
    Persist,
    // Пара исчезла: формы разошлись, коллайдер выключен, объект удалён или сцена сменилась.
    End
};

// Контакт двух объектов сцены. first всегда меньше second, а manifold.normal направлена
// от first ко second.
struct Contact
{
    ObjectId first = 0;
    ObjectId second = 0;
    Manifold manifold;
    // Триггером помечен хотя бы один из коллайдеров.
    bool trigger = false;
};

struct ContactEvent
{
    ContactPhase phase = ContactPhase::Begin;
    Contact contact;
};

// Распознавание контактов объектов сцены: полный перебор пар с отсечением по
// охватывающим прямоугольникам. Трекер помнит пары прошлого вызова, поэтому начало,
// продолжение и завершение контакта сообщаются по одному разу.
class ContactTracker
{
public:
    ContactTracker() = default;

    // Проверяет коллайдеры сцены и возвращает переходы за этот вызов: сначала текущие
    // пары в порядке идентификаторов, затем завершившиеся. Возвращённый вектор живёт
    // до следующего вызова. Передача другой сцены завершает прежние контакты:
    // идентификаторы объектов разных сцен совпадают.
    const std::vector<ContactEvent>& update(const Scene& scene);

private:
    // Коллайдер объекта, его форма в мировых координатах и мир формы для отсечения пар.
    struct Entry
    {
        ObjectId id = 0;
        Collider collider;
        CollisionShape shape;
        Rect bounds;
    };

    void collect(const Scene& scene);
    void checkPair(const Entry& first, const Entry& second);

    std::vector<Entry> entries;
    std::map<std::pair<ObjectId, ObjectId>, Contact> active;
    std::map<std::pair<ObjectId, ObjectId>, Contact> found;
    std::vector<ContactEvent> events;
    const Scene* currentScene = nullptr;
};
}
