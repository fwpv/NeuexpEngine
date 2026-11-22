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

// Распознавание контактов объектов сцены: пары перебираются в порядке левой границы
// охватывающих прямоугольников. Трекер помнит пары прошлого вызова, поэтому начало,
// продолжение и завершение контакта сообщаются по одному разу.
//
// Проверка дискретная: формы сравниваются в положениях на момент вызова. Объект,
// прошедший за один вызов сквозь другой целиком, контакта не даст — ложных событий
// при этом не возникает. Для быстрых объектов путь проверяется отрезком (castSegment);
// непрерывная проверка времени контакта потребовала бы хранения предыдущих положений
// и скорости сближения, которых у трекера нет.
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
    // Проход по парам, чьи проекции на ось x пересекаются: порядок по левой границе
    // охватывающего прямоугольника, затем проверка до первой разделённой пары.
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
