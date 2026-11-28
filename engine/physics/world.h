#pragma once

#include "body.h"
#include "collision/contact_phase.h"
#include "collision/manifold.h"
#include "core/math2d.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace NeuexpEngine
{
class Scene;

// Идентификатор тела внутри мира; не переиспользуется.
using BodyId = std::uint64_t;

// Контакт тел с данными на момент обнаружения, до разрешения, и импульсами решателя
// после него. first < second, normal направлена от first ко second; точки мировые.
struct PhysicsContact
{
    BodyId first = 0;
    BodyId second = 0;
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

// Мир тел с гравитацией, силами, трением и упругостью. Один шаг step() — постоянная
// длительность симуляции: при одинаковом dT результат воспроизводим. Пары тел
// отсекаются охватывающими прямоугольниками, слоями и масками; пары без динамического
// тела не проверяются, поэтому сенсор на движущемся теле не видит статические тела.
// Контакты разрешаются последовательными импульсами: тёплый старт по особенностям форм,
// двухточечные контакты решаются блочно. Упругость считается по скорости до шага,
// поэтому отскок не накачивает энергию. Динамические тела в покое засыпают целыми
// островами и просыпаются от удара, силы или заданной скорости. Позиции интегрируются
// с непрерывной проверкой удара для всех пар тел. Удаление отложено и безопасно
// вместе с обработкой событий. Параметры тел проверяются assert в отладочной сборке.
class PhysicsWorld
{
public:
    PhysicsWorld() = default;
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    // Тело копируется в мир и получает идентификатор. Позиция, скорость, плотность,
    // трение и упругость проверяются assert при добавлении; слои и маска — свободные
    // битовые поля, а неизвестное значение вида тела действует как неподвижное.
    BodyId addBody(const RigidBody& description);
    // Тело сразу перестаёт участвовать в шагах и запросах мира, его контакты
    // завершаются на ближайшем step(), а память освобождается в конце этого шага.
    // Неизвестный и уже удалённый идентификатор игнорируются.
    void destroyBody(BodyId id);
    RigidBody* body(BodyId id);
    const RigidBody* body(BodyId id) const;
    // Живые тела в порядке добавления; ожидающие удаления не входят. Указатели
    // действуют до следующих addBody() и destroyBody().
    std::vector<RigidBody*> bodies();
    std::vector<const RigidBody*> bodies() const;
    std::size_t bodyCount() const;

    // Один шаг симуляции; dT конечен и положителен. Порядок: интегрирование скоростей,
    // поиск контактов, пробуждение от движущихся тел, разрешение скоростей, интегрирование
    // положений с непрерывной проверкой удара по пути, устранение проникновения, подсчёт
    // времени сна, сброс сил и формирование событий.
    void step(float deltaTime);
    // Переходы контактов за последний шаг: пары текущего шага в порядке идентификаторов,
    // затем завершившиеся. Вектор живёт до следующего step().
    const std::vector<PhysicsContactEvent>& contactEvents() const { return events; }

    // Записывает положение и угол тел в трансформации объектов сцены по body.object;
    // объекты, которых уже нет в сцене, пропускаются.
    void sync(Scene& scene) const;

    // Ускорение свободного падения: единицы мира в секунду за секунду. Поле публично,
    // как параметры камеры: это настройка мира без проверок, применяется следующим шагом.
    Vector2 gravity = {0, 900};

private:
    struct Slot
    {
        BodyId id = 0;
        RigidBody body;
        bool removalPending = false;
        // Скорость на начало шага, до гравитации и сил: по ней считается упругость,
        // иначе удар добавлял бы к скорости один шаг ускорения.
        Vector2 previousVelocity;
        float previousAngularVelocity = 0;
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
        BodyId first = 0;
        BodyId second = 0;
        RigidBody* firstBody = nullptr;
        RigidBody* secondBody = nullptr;
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

    // Тело с формой в мире для поиска пар по левой границе.
    struct BroadEntry
    {
        BodyId id = 0;
        RigidBody* body = nullptr;
        CollisionShape shape;
        Rect bounds;
        Vector2 previousVelocity;
        float previousAngularVelocity = 0;
    };

    RigidBody* findBody(BodyId id);
    void integrateVelocities(float deltaTime);
    void collectContacts();
    // Будит тела, которых касаются движущиеся тела, и распространяет пробуждение
    // по цепочке контактов.
    void wakeBodies();
    // Подсчитывает время неподвижности и усыпляет тела с малой скоростью.
    void updateSleeping(float deltaTime);
    void checkPair(const BroadEntry& first, const BroadEntry& second);
    // Переносит импульсы прошлого шага в совпавшие точки контакта.
    void warmStartContact(SolveContact& contact);
    void applyWarmStart();
    void storeWarmStart();
    // Одновременное решение нормальной связи двух контактных точек (блочный решатель).
    bool solveNormalBlock(SolveContact& contact);
    void solveVelocities();
    // Интегрирование положений с непрерывной проверкой: быстрые тела останавливаются
    // в точке первого касания с любым телом мира, а не только со статикой.
    void integratePositions(float deltaTime);
    void solvePositions();
    void resetForces();
    void emitEvents();
    void eraseRemoved();
    PhysicsContact makeContact(const SolveContact& contact) const;

    std::vector<Slot> slots;
    BodyId nextBodyId = 1;
    std::vector<BroadEntry> entries;
    std::vector<std::size_t> order;
    std::map<std::pair<BodyId, BodyId>, SolveContact> contacts;
    std::map<std::pair<BodyId, BodyId>, PhysicsContact> active;
    std::map<std::pair<BodyId, BodyId>, WarmContact> warmContacts;
    std::vector<PhysicsContactEvent> events;
};
}
