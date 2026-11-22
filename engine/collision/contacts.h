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

// Распознавание контактов сцены: пары перебираются по левой границе охватывающих
// прямоугольников, пары прошлого вызова запоминаются, поэтому начало, продолжение
// и завершение контакта приходят по одному разу. Проверка дискретная: пролетевший
// за вызов объект контакта не даст (ложных событий при этом не бывает) — для быстрых
// объектов путь проверяется отрезком castSegment.
class ContactTracker
{
public:
    ContactTracker() = default;

    // Возвращает переходы за вызов: текущие пары в порядке идентификаторов, затем
    // завершившиеся; вектор живёт до следующего вызова. Другая сцена завершает прежние
    // контакты, потому что идентификаторы объектов разных сцен совпадают.
    const std::vector<ContactEvent>& update(const Scene& scene);

private:
    // Коллайдер объекта, его форма в мире и охватывающий прямоугольник для отсечения пар.
    struct Entry
    {
        ObjectId id = 0;
        Collider collider;
        CollisionShape shape;
        Rect bounds;
    };

    void collect(const Scene& scene);
    // Проверка пар в порядке левой границы: проход прерывается, когда проекции на x разошлись.
    void checkPairs();
    void checkPair(const Entry& first, const Entry& second);

    std::vector<Entry> entries;
    // Индексы entries в порядке левой границы: переиспользуется между вызовами.
    std::vector<std::size_t> order;
    std::map<std::pair<ObjectId, ObjectId>, Contact> active;
    std::map<std::pair<ObjectId, ObjectId>, Contact> found;
    std::vector<ContactEvent> events;
    const Scene* currentScene = nullptr;
};
}
