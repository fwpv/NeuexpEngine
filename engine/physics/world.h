#pragma once

#include "collision/manifold.h"
#include "collision/shape.h"
#include "core/transform.h"
#include "core/geometry.h"
#include "core/math2d.h"
#include "scene/object_id.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace NeuexpEngine
{
class GameObject;

// Фаза контакта пары.
enum class ContactPhase
{
    // Пара появилась в этом шаге.
    Begin,
    // Пара была в прошлом шаге и остаётся; данные контакта относятся к этому шагу.
    Persist,
    // Пара исчезла: формы разошлись, пара запрещена или объект удалён.
    End
};

// Контакт тел с данными на момент обнаружения, до разрешения, и импульсами решателя
// после него. first < second, normal направлена от first ко second; точки мировые.
struct PhysicsContact
{
    ObjectId first = 0;
    ObjectId second = 0;
    Vector2 normal = {1, 0};
    float depth = 0;
    std::array<Vector2, 2> points = {};
    int pointCount = 0;
    bool sensor = false;
    // Суммарные за шаг импульсы нормали и трения; у сенсоров нули.
    float normalImpulse = 0;
    float tangentImpulse = 0;
};

struct PhysicsContactEvent
{
    ContactPhase phase = ContactPhase::Begin;
    PhysicsContact contact;
};

// Физический мир шага: шаг получает объекты, которые приложение обновляло в этом
// кадре. Объект участвует, если у него включён коллайдер; форму, слои, маску, сенсор
// и поверхностные свойства даёт коллайдер, поза — трансформация объекта (её центр —
// центр масс и точка вращения тела), динамику — PhysicsBody. Объект без тела остаётся
// неподвижной геометрией; пары из двух неподвижных объектов не проверяются, а пары
// с кинематическим телом сообщаются без отталкивания. Шаг читает трансформации
// и записывает позы обратно, поэтому после шага геометрия объектов актуальна.
// Длительность шага задаёт приложение: при фиксированном dT результат воспроизводим,
// при переменном — нет; собственного накопителя времени у мира нет.
// Контакты разрешаются последовательными импульсами: тёплый старт по особенностям форм,
// двухточечные контакты решаются блочно. Упругость считается по скорости до шага,
// поэтому отскок не накачивает энергию. Динамические тела в покое засыпают целыми
// островами и просыпаются от удара, силы или заданной скорости. Позиции интегрируются
// с непрерывной проверкой удара для динамических тел. Параметры тел проверяются assert
// в отладочной сборке.
class PhysicsWorld
{
public:
    PhysicsWorld() = default;
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    // Один шаг симуляции; dT конечен и положителен. objects — живые объекты этого
    // кадра: у каждого включённого коллайдера поза читается из трансформации, а поза
    // тела записывается обратно. Мир не владеет объектами и не хранит указатели на них
    // между шагами, поэтому удаление объекта после шага безопасно.
    void step(const std::vector<GameObject*>& objects, float deltaTime);
    // Переходы контактов за последний шаг: пары текущего шага в порядке идентификаторов,
    // затем завершившиеся. Вектор живёт до следующего step().
    const std::vector<PhysicsContactEvent>& contactEvents() const { return events; }
    // Немедленно забывает контакты без событий: сцена очищается целиком.
    void clear();

    // Ускорение свободного падения: единицы мира в секунду за секунду. Поле публично,
    // как параметры камеры: это настройка мира без проверок, применяется следующим шагом.
    Vector2 gravity = {0, 900};

private:
    // Тело на время шага: объект, его форма на начало шага и производные массы.
    struct Body
    {
        GameObject* object = nullptr;
        ObjectId id = 0;
        CollisionShape shape;
        Rect bounds;
        Vector2 previousVelocity;
        float previousAngularVelocity = 0;
        float inverseMass = 0;
        float inverseInertia = 0;
        // Кинематическое тело, которое приложение перенесло трансформацией с прошлого шага:
        // такой перенос тоже будит лежащие на пути тела.
        bool transformMoved = false;
    };

    // Контактная точка решателя: якоря в осях обоих тел, плечи на момент обнаружения,
    // накопленные импульсы и целевая скорость отскока.
    struct SolvePoint
    {
        Vector2 localFirst;
        Vector2 localSecond;
        Vector2 armFirst;
        Vector2 armSecond;
        float separation = 0;
        float targetVelocity = 0;
        float normalImpulse = 0;
        float tangentImpulse = 0;
        // Идентификатор особенности форм для тёплого старта.
        std::uint16_t id = 0;
    };

    struct SolveContact
    {
        ObjectId first = 0;
        ObjectId second = 0;
        Body* firstBody = nullptr;
        Body* secondBody = nullptr;
        ContactManifold manifold;
        bool sensor = false;
        float friction = 0;
        std::array<SolvePoint, 2> points;
        int count = 0;
    };

    // Импульсы контактной точки прошлого шага: по ним решатель начинает проход
    // с уже найденного равновесия, поэтому стопки тел не дрожат. Точки сопоставляются
    // по идентификатору особенности форм, а не по близости.
    struct WarmPoint
    {
        std::uint16_t id = 0;
        // Якоря нужны, когда особенности форм сменились: точка переносится ближайшей.
        Vector2 localFirst;
        Vector2 localSecond;
        float normalImpulse = 0;
        float tangentImpulse = 0;
    };

    struct WarmContact
    {
        std::array<WarmPoint, 2> points;
        int count = 0;
    };

    // Скорость точки тела и проверка его участия в решателе.
    static bool bodyIsActive(const Body& body);
    static Vector2 velocityAt(const Body& body, Vector2 arm);
    static void applyPairImpulse(Body& first, Body& second, Vector2 impulse, Vector2 armFirst,
        Vector2 armSecond);

    void collectBodies(const std::vector<GameObject*>& objects);
    void integrateVelocities(float deltaTime);
    void collectContacts();
    // Будит тела, которых касаются движущиеся тела, и распространяет пробуждение
    // по цепочке контактов.
    void wakeBodies();
    // Подсчитывает время неподвижности и усыпляет тела с малой скоростью.
    void updateSleeping(float deltaTime);
    void checkPair(std::size_t firstIndex, std::size_t secondIndex);
    // Переносит импульсы прошлого шага в совпавшие точки контакта.
    void warmStartContact(SolveContact& contact);
    void applyWarmStart();
    void storeWarmStart();
    // Одновременное решение нормальной связи двух контактных точек (блочный решатель).
    bool solveNormalBlock(SolveContact& contact);
    void solveVelocities();
    // Интегрирование положений с непрерывной проверкой: быстрые тела останавливаются
    // в точке первого касания с любым телом мира, а не только со статикой; позы тел
    // записываются в трансформации объектов.
    void integratePositions(float deltaTime);
    void solvePositions();
    void resetForces();
    void emitEvents();
    PhysicsContact makeContact(const SolveContact& contact) const;

    std::vector<Body> bodies;
    std::vector<std::size_t> order;
    // Поза кинематических тел на начало прошлого шага: по ней видно перенос приложением.
    std::map<ObjectId, Transform2> previousPoses;
    std::map<std::pair<ObjectId, ObjectId>, SolveContact> contacts;
    std::map<std::pair<ObjectId, ObjectId>, PhysicsContact> active;
    std::map<std::pair<ObjectId, ObjectId>, WarmContact> warmContacts;
    std::vector<PhysicsContactEvent> events;
};
}
