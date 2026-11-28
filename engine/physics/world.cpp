#include "world.h"
#include "scene/scene.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <set>
#include <utility>

namespace NeuexpEngine
{
namespace
{
// Ограниченное число итераций решателя: скорость тел стабилизируется за несколько
// проходов, а стоимость шага остаётся предсказуемой.
constexpr int VelocityIterations = 12;
constexpr int PositionIterations = 3;
// Доля устранённого проникновения за итерацию и запас, который проникновением не считается.
constexpr float PositionFactor = 0.2F;
constexpr float LinearSlop = 0.1F;
constexpr float MaximumCorrection = 4.0F;
// Ниже этой скорости сближения удар не возвращает энергию: иначе покоящееся тело дрожит.
constexpr float RestitutionThreshold = 20.0F;
// Порог сна: тело с малой линейной и угловой скоростью засыпает, и мир его
// не интегрирует, пока его не разбудят. Значения в единицах мира и радианах.
constexpr float SleepLinearSpeed = 0.5F;
constexpr float SleepAngularSpeed = 0.02F;
constexpr float SleepTime = 0.5F;
// Квадрат допустимого сдвига якорей при переносе импульсов на сменившуюся особенность.
constexpr float WarmStartTolerance = 4.0F;
// Небольшое проникновение в точке удара: контакт получается устойчивым, как у обычного
// решателя, а на экране такая глубина незаметна.
constexpr float ImpactMargin = 0.2F;

// Тело участвует в решателе и непрерывной проверке: динамическое и не спящее
// или кинематическое в движении.
bool bodyIsActive(const RigidBody& body)
{
    if (body.type == BodyType::Dynamic) {
        return !body.sleeping;
    }
    if (body.type == BodyType::Kinematic) {
        return body.velocity.x != 0 || body.velocity.y != 0 || body.angularVelocity != 0;
    }
    return false;
}

Vector2 velocityAt(Vector2 linearVelocity, float angularVelocity, Vector2 arm)
{
    return linearVelocity + Vector2{-angularVelocity * arm.y, angularVelocity * arm.x};
}

Vector2 velocityAt(const RigidBody& body, Vector2 arm)
{
    return velocityAt(body.velocity, body.angularVelocity, arm);
}

void applyPairImpulse(RigidBody& first, RigidBody& second, Vector2 impulse, Vector2 armFirst, Vector2 armSecond)
{
    first.velocity -= impulse * first.inverseMass();
    first.angularVelocity -= cross(armFirst, impulse) * first.inverseInertia();
    second.velocity += impulse * second.inverseMass();
    second.angularVelocity += cross(armSecond, impulse) * second.inverseInertia();
}

// Ненулевая эффективная масса нормальной связи; ноль означает, что оба тела неподвижны
// или кинематичны в этой паре.
float normalMass(const RigidBody& first, const RigidBody& second, Vector2 armFirst, Vector2 armSecond, Vector2 normal)
{
    const float armFirstCross = cross(armFirst, normal);
    const float armSecondCross = cross(armSecond, normal);
    return first.inverseMass() + second.inverseMass() + first.inverseInertia() * armFirstCross * armFirstCross
        + second.inverseInertia() * armSecondCross * armSecondCross;
}

// Мировая форма тела в заданной позе: используется для поиска времени удара.
CollisionShape shapeAtPose(const RigidBody& body, Vector2 position, float angle)
{
    RigidBody probe = body;
    probe.position = position;
    probe.angle = angle;
    return probe.worldShape();
}

// Охватывающий прямоугольник всего пути перемещения формы.
Rect pathBounds(const CollisionShape& start, const CollisionShape& end)
{
    const Rect startBounds = bounds(start);
    const Rect endBounds = bounds(end);
    const float left = std::min(startBounds.position.x, endBounds.position.x);
    const float top = std::min(startBounds.position.y, endBounds.position.y);
    const float right = std::max(startBounds.position.x + startBounds.size.width,
        endBounds.position.x + endBounds.size.width);
    const float bottom = std::max(startBounds.position.y + startBounds.size.height,
        endBounds.position.y + endBounds.size.height);
    return Rect{left, top, right - left, bottom - top};
}

// Минимальная ширина формы: по ней выбирается шаг выборки пути при поиске удара.
float minimalWidth(const CollisionShape& shape)
{
    if (shape.kind() == CollisionShape::Kind::Circle) {
        return shape.circle().radius * 2;
    }
    if (shape.kind() == CollisionShape::Kind::Rect) {
        return std::min(shape.rect().size.width, shape.rect().size.height);
    }
    return std::min(shape.box().size.width, shape.box().size.height);
}
}

BodyId PhysicsWorld::addBody(const RigidBody& description)
{
    // Параметры физики должны быть конечными и в допустимых диапазонах: иначе
    // отрицательное трение даёт NaN в пределе, а упругость вне [0, 1] накачивает энергию.
    assert(std::isfinite(description.position.x) && std::isfinite(description.position.y)
        && std::isfinite(description.angle));
    assert(std::isfinite(description.velocity.x) && std::isfinite(description.velocity.y)
        && std::isfinite(description.angularVelocity));
    assert(std::isfinite(description.density) && description.density > 0);
    assert(std::isfinite(description.friction) && description.friction >= 0 && description.friction <= 1);
    assert(std::isfinite(description.restitution) && description.restitution >= 0
        && description.restitution <= 1);
    assert(std::isfinite(description.linearDamping) && description.linearDamping >= 0);
    assert(std::isfinite(description.angularDamping) && description.angularDamping >= 0);
    assert(std::isfinite(description.gravityScale));
    Slot slot;
    slot.id = nextBodyId;
    ++nextBodyId;
    slot.body = description;
    slots.push_back(slot);
    return slot.id;
}

void PhysicsWorld::destroyBody(BodyId id)
{
    for (Slot& slot : slots) {
        if (slot.id == id) {
            slot.removalPending = true;
            return;
        }
    }
}

RigidBody* PhysicsWorld::body(BodyId id)
{
    return findBody(id);
}

const RigidBody* PhysicsWorld::body(BodyId id) const
{
    for (const Slot& slot : slots) {
        if (slot.id == id && !slot.removalPending) {
            return &slot.body;
        }
    }
    return nullptr;
}

RigidBody* PhysicsWorld::findBody(BodyId id)
{
    for (Slot& slot : slots) {
        if (slot.id == id && !slot.removalPending) {
            return &slot.body;
        }
    }
    return nullptr;
}

std::vector<RigidBody*> PhysicsWorld::bodies()
{
    std::vector<RigidBody*> result;
    for (Slot& slot : slots) {
        if (!slot.removalPending) {
            result.push_back(&slot.body);
        }
    }
    return result;
}

std::vector<const RigidBody*> PhysicsWorld::bodies() const
{
    std::vector<const RigidBody*> result;
    for (const Slot& slot : slots) {
        if (!slot.removalPending) {
            result.push_back(&slot.body);
        }
    }
    return result;
}

std::size_t PhysicsWorld::bodyCount() const
{
    return static_cast<std::size_t>(
        std::count_if(slots.begin(), slots.end(), [](const Slot& slot) { return !slot.removalPending; }));
}

void PhysicsWorld::step(float deltaTime)
{
    assert(deltaTime > 0 && std::isfinite(deltaTime));
    events.clear();
    integrateVelocities(deltaTime);
    collectContacts();
    wakeBodies();
    solveVelocities();
    integratePositions(deltaTime);
    solvePositions();
    updateSleeping(deltaTime);
    storeWarmStart();
    resetForces();
    emitEvents();
    eraseRemoved();
}

void PhysicsWorld::sync(Scene& scene) const
{
    for (const Slot& slot : slots) {
        if (slot.removalPending || slot.body.object == 0) {
            continue;
        }
        GameObject* object = scene.find(slot.body.object);
        if (object == nullptr) {
            continue;
        }
        Transform2& transform = object->transform();
        transform.position = slot.body.position;
        transform.rotation = slot.body.angle;
    }
}

void PhysicsWorld::integrateVelocities(float deltaTime)
{
    for (Slot& slot : slots) {
        if (slot.removalPending) {
            continue;
        }
        RigidBody& body = slot.body;
        slot.previousVelocity = body.velocity;
        slot.previousAngularVelocity = body.angularVelocity;
        if (body.type != BodyType::Dynamic) {
            continue;
        }
        // Внешняя сила, заданная скорость или импульс будят тело.
        if (body.sleeping && (body.force.x != 0 || body.force.y != 0 || body.torque != 0
                || body.velocity.x != 0 || body.velocity.y != 0 || body.angularVelocity != 0)) {
            body.sleeping = false;
            body.sleepTime = 0;
        }
        if (body.sleeping) {
            continue;
        }
        body.velocity += (gravity * body.gravityScale + body.force * body.inverseMass()) * deltaTime;
        body.angularVelocity += body.torque * body.inverseInertia() * deltaTime;
        body.velocity *= 1 / (1 + body.linearDamping * deltaTime);
        body.angularVelocity /= 1 + body.angularDamping * deltaTime;
    }
}

void PhysicsWorld::collectContacts()
{
    contacts.clear();
    entries.clear();
    for (Slot& slot : slots) {
        if (slot.removalPending) {
            continue;
        }
        BroadEntry entry;
        entry.id = slot.id;
        entry.body = &slot.body;
        entry.shape = slot.body.worldShape();
        entry.bounds = bounds(entry.shape);
        entry.previousVelocity = slot.previousVelocity;
        entry.previousAngularVelocity = slot.previousAngularVelocity;
        entries.push_back(entry);
    }
    order.resize(entries.size());
    for (std::size_t index = 0; index < order.size(); ++index) {
        order[index] = index;
    }
    std::sort(order.begin(), order.end(), [this](std::size_t first, std::size_t second) {
        const BroadEntry& firstEntry = entries[first];
        const BroadEntry& secondEntry = entries[second];
        if (firstEntry.bounds.position.x != secondEntry.bounds.position.x) {
            return firstEntry.bounds.position.x < secondEntry.bounds.position.x;
        }
        return firstEntry.id < secondEntry.id;
    });
    for (std::size_t first = 0; first < order.size(); ++first) {
        const BroadEntry& entry = entries[order[first]];
        const float right = entry.bounds.position.x + entry.bounds.size.width;
        for (std::size_t second = first + 1; second < order.size(); ++second) {
            const BroadEntry& other = entries[order[second]];
            if (other.bounds.position.x > right) {
                break;
            }
            if (!intersects(entry.bounds, other.bounds)) {
                continue;
            }
            checkPair(entry, other);
        }
    }
}

void PhysicsWorld::checkPair(const BroadEntry& a, const BroadEntry& b)
{
    RigidBody& bodyA = *a.body;
    RigidBody& bodyB = *b.body;
    // Неподвижные и кинематические тела не отталкиваются друг от друга.
    if (bodyA.type != BodyType::Dynamic && bodyB.type != BodyType::Dynamic) {
        return;
    }
    if (!layersMatch(bodyA.layer, bodyA.mask, bodyB.layer, bodyB.mask)) {
        return;
    }
    const bool firstIsSmaller = a.id < b.id;
    const CollisionShape& firstShape = firstIsSmaller ? a.shape : b.shape;
    const CollisionShape& secondShape = firstIsSmaller ? b.shape : a.shape;
    ContactManifold manifold;
    if (!detectContact(firstShape, secondShape, manifold)) {
        return;
    }
    SolveContact contact;
    contact.first = firstIsSmaller ? a.id : b.id;
    contact.second = firstIsSmaller ? b.id : a.id;
    contact.firstBody = firstIsSmaller ? &bodyA : &bodyB;
    contact.secondBody = firstIsSmaller ? &bodyB : &bodyA;
    contact.manifold = manifold;
    contact.sensor = bodyA.sensor || bodyB.sensor;
    contact.friction = std::sqrt(bodyA.friction * bodyB.friction);
    const float restitution = std::max(bodyA.restitution, bodyB.restitution);
    contact.count = manifold.count;
    const Vector2 firstPreviousVelocity = firstIsSmaller ? a.previousVelocity : b.previousVelocity;
    const float firstPreviousAngular = firstIsSmaller ? a.previousAngularVelocity : b.previousAngularVelocity;
    const Vector2 secondPreviousVelocity = firstIsSmaller ? b.previousVelocity : a.previousVelocity;
    const float secondPreviousAngular = firstIsSmaller ? b.previousAngularVelocity : a.previousAngularVelocity;
    for (int index = 0; index < manifold.count; ++index) {
        SolvePoint& point = contact.points[static_cast<std::size_t>(index)];
        const ContactPoint& contactPoint = manifold.points[static_cast<std::size_t>(index)];
        point.separation = -contactPoint.depth;
        point.armFirst = contactPoint.position - contact.firstBody->position;
        point.armSecond = contactPoint.position - contact.secondBody->position;
        point.localFirst = rotated(point.armFirst, -contact.firstBody->angle);
        point.localSecond = rotated(point.armSecond, -contact.secondBody->angle);
        point.id = contactPoint.id;
        // Упругость считается по скорости до шага: иначе к удару добавляется g * dT
        // и тело с упругостью 1 постепенно набирает высоту.
        const Vector2 relative = velocityAt(secondPreviousVelocity, secondPreviousAngular, point.armSecond)
            - velocityAt(firstPreviousVelocity, firstPreviousAngular, point.armFirst);
        const float normalVelocity = dot(relative, manifold.normal);
        point.targetVelocity = normalVelocity < -RestitutionThreshold ? -restitution * normalVelocity : 0;
    }
    const std::pair<BodyId, BodyId> key{contact.first, contact.second};
    if (active.find(key) != active.end()) {
        warmStartContact(contact);
    }
    contacts.emplace(key, contact);
}

void PhysicsWorld::warmStartContact(SolveContact& contact)
{
    // Импульсы прошлого шага переносятся только точкам с той же особенностью форм:
    // новая грань или вершина начинает с нуля, а устойчивый контакт сохраняет импульсы.
    const std::pair<BodyId, BodyId> key{contact.first, contact.second};
    const auto previous = warmContacts.find(key);
    if (previous == warmContacts.end()) {
        return;
    }
    std::array<bool, 2> used = {false, false};
    for (int index = 0; index < contact.count; ++index) {
        SolvePoint& point = contact.points[static_cast<std::size_t>(index)];
        int best = -1;
        // Сначала та же особенность форм: устойчивый контакт сохраняет импульсы точно.
        for (int candidate = 0; candidate < previous->second.count; ++candidate) {
            if (used[static_cast<std::size_t>(candidate)]) {
                continue;
            }
            if (previous->second.points[static_cast<std::size_t>(candidate)].id == point.id) {
                best = candidate;
                break;
            }
        }
        // Особенность сменилась (например, контакт перешёл на соседнюю грань):
        // импульсы переносятся ближайшей точке, чтобы стопка не теряла равновесие.
        if (best < 0) {
            float bestDistance = WarmStartTolerance;
            for (int candidate = 0; candidate < previous->second.count; ++candidate) {
                if (used[static_cast<std::size_t>(candidate)]) {
                    continue;
                }
                const WarmPoint& warm = previous->second.points[static_cast<std::size_t>(candidate)];
                const float distance = lengthSquared(point.localFirst - warm.localFirst)
                    + lengthSquared(point.localSecond - warm.localSecond);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    best = candidate;
                }
            }
        }
        if (best < 0) {
            continue;
        }
        used[static_cast<std::size_t>(best)] = true;
        const WarmPoint& warm = previous->second.points[static_cast<std::size_t>(best)];
        point.normalImpulse = warm.normalImpulse;
        point.tangentImpulse = warm.tangentImpulse;
    }
}

void PhysicsWorld::applyWarmStart()
{
    for (auto& entry : contacts) {
        SolveContact& contact = entry.second;
        if (contact.sensor
            || (!bodyIsActive(*contact.firstBody) && !bodyIsActive(*contact.secondBody))) {
            continue;
        }
        RigidBody& first = *contact.firstBody;
        RigidBody& second = *contact.secondBody;
        const Vector2 normal = contact.manifold.normal;
        const Vector2 tangent{-normal.y, normal.x};
        for (int index = 0; index < contact.count; ++index) {
            const SolvePoint& point = contact.points[static_cast<std::size_t>(index)];
            applyPairImpulse(first, second, normal * point.normalImpulse + tangent * point.tangentImpulse,
                point.armFirst, point.armSecond);
        }
    }
}

void PhysicsWorld::storeWarmStart()
{
    warmContacts.clear();
    for (const auto& entry : contacts) {
        const SolveContact& contact = entry.second;
        if (contact.sensor
            || (!bodyIsActive(*contact.firstBody) && !bodyIsActive(*contact.secondBody))) {
            continue;
        }
        WarmContact warm;
        warm.count = contact.count;
        for (int index = 0; index < contact.count; ++index) {
            const SolvePoint& point = contact.points[static_cast<std::size_t>(index)];
            WarmPoint& stored = warm.points[static_cast<std::size_t>(index)];
            stored.id = point.id;
            stored.localFirst = point.localFirst;
            stored.localSecond = point.localSecond;
            stored.normalImpulse = point.normalImpulse;
            stored.tangentImpulse = point.tangentImpulse;
        }
        warmContacts.emplace(entry.first, warm);
    }
}

void PhysicsWorld::wakeBodies()
{
    // Пробуждение распространяется по цепочке контактов: удар в стопку будит всю стопку.
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& entry : contacts) {
            SolveContact& contact = entry.second;
            if (contact.sensor) {
                continue;
            }
            RigidBody& first = *contact.firstBody;
            RigidBody& second = *contact.secondBody;
            if (bodyIsActive(first) && second.sleeping) {
                second.sleeping = false;
                second.sleepTime = 0;
                changed = true;
            }
            if (bodyIsActive(second) && first.sleeping) {
                first.sleeping = false;
                first.sleepTime = 0;
                changed = true;
            }
        }
    }
}

void PhysicsWorld::updateSleeping(float deltaTime)
{
    for (Slot& slot : slots) {
        if (slot.removalPending) {
            continue;
        }
        RigidBody& body = slot.body;
        if (body.type != BodyType::Dynamic || body.sleeping) {
            continue;
        }
        // Внешняя сила (ветер, притяжение) не даёт телу уснуть, даже если его скорость
        // пока мала: иначе тело проигнорирует силу.
        if (body.force.x != 0 || body.force.y != 0 || body.torque != 0
            || lengthSquared(body.velocity) > SleepLinearSpeed * SleepLinearSpeed
            || std::fabs(body.angularVelocity) > SleepAngularSpeed) {
            body.sleepTime = 0;
            continue;
        }
        body.sleepTime += deltaTime;
    }
    // Связные по контактам динамические тела образуют остров и засыпают целиком:
    // иначе одно проснувшееся тело будило бы соседей и стопка никогда не успокаивалась.
    const std::size_t count = slots.size();
    std::vector<std::size_t> parent(count);
    for (std::size_t index = 0; index < count; ++index) {
        parent[index] = index;
    }
    const auto root = [&parent](std::size_t index) {
        while (parent[index] != index) {
            parent[index] = parent[parent[index]];
            index = parent[index];
        }
        return index;
    };
    const auto unite = [&root, &parent](std::size_t first, std::size_t second) {
        const std::size_t firstRoot = root(first);
        const std::size_t secondRoot = root(second);
        if (firstRoot != secondRoot) {
            parent[secondRoot] = firstRoot;
        }
    };
    const auto slotIndexOf = [this](const RigidBody* body) -> std::size_t {
        for (std::size_t index = 0; index < slots.size(); ++index) {
            if (&slots[index].body == body) {
                return index;
            }
        }
        return slots.size();
    };
    for (const auto& entry : contacts) {
        const SolveContact& contact = entry.second;
        if (contact.sensor || contact.firstBody->type != BodyType::Dynamic
            || contact.secondBody->type != BodyType::Dynamic) {
            continue;
        }
        const std::size_t firstIndex = slotIndexOf(contact.firstBody);
        const std::size_t secondIndex = slotIndexOf(contact.secondBody);
        if (firstIndex < count && secondIndex < count) {
            unite(firstIndex, secondIndex);
        }
    }
    std::vector<bool> ready(count, false);
    for (std::size_t index = 0; index < count; ++index) {
        const Slot& slot = slots[index];
        const RigidBody& body = slot.body;
        if (slot.removalPending || body.type != BodyType::Dynamic || body.sleeping
            || body.sleepTime < SleepTime) {
            continue;
        }
        ready[index] = true;
    }
    for (std::size_t index = 0; index < count; ++index) {
        if (!ready[index]) {
            continue;
        }
        // Засыпает весь остров: все его тела должны быть готовы.
        bool islandReady = true;
        for (std::size_t other = 0; other < count; ++other) {
            if (root(other) != root(index)) {
                continue;
            }
            const Slot& slot = slots[other];
            const RigidBody& body = slot.body;
            if (slot.removalPending || body.type != BodyType::Dynamic) {
                continue;
            }
            if (body.sleeping) {
                continue;
            }
            if (!ready[other]) {
                islandReady = false;
                break;
            }
        }
        if (!islandReady) {
            continue;
        }
        for (std::size_t other = 0; other < count; ++other) {
            if (root(other) != root(index)) {
                continue;
            }
            RigidBody& body = slots[other].body;
            if (slots[other].removalPending || body.type != BodyType::Dynamic || body.sleeping) {
                continue;
            }
            body.sleeping = true;
            body.sleepTime = SleepTime;
            body.velocity = {};
            body.angularVelocity = 0;
        }
    }
}

bool PhysicsWorld::solveNormalBlock(SolveContact& contact)
{
    // Обе нормальные связи решаются одновременно: для двухточечного контакта так
    // учитывается взаимное влияние точек, и стопки тел не расходятся.
    SolvePoint& firstPoint = contact.points[0];
    SolvePoint& secondPoint = contact.points[1];
    RigidBody& first = *contact.firstBody;
    RigidBody& second = *contact.secondBody;
    const Vector2 normal = contact.manifold.normal;
    const float inverseMass = first.inverseMass() + second.inverseMass();
    const float inverseInertiaFirst = first.inverseInertia();
    const float inverseInertiaSecond = second.inverseInertia();
    const float firstArmFirst = cross(firstPoint.armFirst, normal);
    const float firstArmSecond = cross(firstPoint.armSecond, normal);
    const float secondArmFirst = cross(secondPoint.armFirst, normal);
    const float secondArmSecond = cross(secondPoint.armSecond, normal);
    const float k11 = inverseMass + inverseInertiaFirst * firstArmFirst * firstArmFirst
        + inverseInertiaSecond * firstArmSecond * firstArmSecond;
    const float k22 = inverseMass + inverseInertiaFirst * secondArmFirst * secondArmFirst
        + inverseInertiaSecond * secondArmSecond * secondArmSecond;
    const float k12 = inverseMass + inverseInertiaFirst * firstArmFirst * secondArmFirst
        + inverseInertiaSecond * firstArmSecond * secondArmSecond;
    const float determinant = k11 * k22 - k12 * k12;
    if (!(k11 > 0) || !(determinant > 0)) {
        return false;
    }
    const float firstVelocity = dot(
        velocityAt(second, firstPoint.armSecond) - velocityAt(first, firstPoint.armFirst), normal);
    const float secondVelocity = dot(
        velocityAt(second, secondPoint.armSecond) - velocityAt(first, secondPoint.armFirst), normal);
    const float firstTarget = firstPoint.targetVelocity - firstVelocity;
    const float secondTarget = secondPoint.targetVelocity - secondVelocity;
    const float accumulatedFirst = firstPoint.normalImpulse;
    const float accumulatedSecond = secondPoint.normalImpulse;
    float impulseFirst = 0;
    float impulseSecond = 0;
    bool solved = false;
    // Набор допустимых решений LCP: обе точки активны, только одна, ни одной.
    const float bothFirst = accumulatedFirst
        + (k22 * firstTarget - k12 * secondTarget) / determinant;
    const float bothSecond = accumulatedSecond
        + (k11 * secondTarget - k12 * firstTarget) / determinant;
    if (bothFirst >= 0 && bothSecond >= 0) {
        impulseFirst = bothFirst;
        impulseSecond = bothSecond;
        solved = true;
    }
    if (!solved) {
        const float onlyFirst = accumulatedFirst + (firstTarget + k12 * accumulatedSecond) / k11;
        if (onlyFirst >= 0) {
            const float secondAfter = secondVelocity + k12 * (onlyFirst - accumulatedFirst)
                - k22 * accumulatedSecond;
            if (secondAfter >= secondPoint.targetVelocity) {
                impulseFirst = onlyFirst;
                solved = true;
            }
        }
    }
    if (!solved) {
        const float onlySecond = accumulatedSecond + (secondTarget + k12 * accumulatedFirst) / k22;
        if (onlySecond >= 0) {
            const float firstAfter = firstVelocity - k11 * accumulatedFirst
                + k12 * (onlySecond - accumulatedSecond);
            if (firstAfter >= firstPoint.targetVelocity) {
                impulseSecond = onlySecond;
                solved = true;
            }
        }
    }
    if (!solved) {
        const float firstAfter = firstVelocity - k11 * accumulatedFirst - k12 * accumulatedSecond;
        const float secondAfter = secondVelocity - k12 * accumulatedFirst - k22 * accumulatedSecond;
        if (firstAfter >= firstPoint.targetVelocity && secondAfter >= secondPoint.targetVelocity) {
            solved = true;
        }
    }
    if (!solved) {
        return false;
    }
    firstPoint.normalImpulse = impulseFirst;
    secondPoint.normalImpulse = impulseSecond;
    applyPairImpulse(first, second, normal * (impulseFirst - accumulatedFirst),
        firstPoint.armFirst, firstPoint.armSecond);
    applyPairImpulse(first, second, normal * (impulseSecond - accumulatedSecond),
        secondPoint.armFirst, secondPoint.armSecond);
    return true;
}

void PhysicsWorld::solveVelocities()
{
    applyWarmStart();
    for (int iteration = 0; iteration < VelocityIterations; ++iteration) {
        for (auto& entry : contacts) {
            SolveContact& contact = entry.second;
            if (contact.sensor || (!bodyIsActive(*contact.firstBody) && !bodyIsActive(*contact.secondBody))) {
                continue;
            }
            RigidBody& first = *contact.firstBody;
            RigidBody& second = *contact.secondBody;
            const Vector2 normal = contact.manifold.normal;
            if (!(contact.count == 2 && solveNormalBlock(contact))) {
                for (int index = 0; index < contact.count; ++index) {
                    SolvePoint& point = contact.points[static_cast<std::size_t>(index)];
                    const float normalVelocity = dot(
                        velocityAt(second, point.armSecond) - velocityAt(first, point.armFirst), normal);
                    const float normalEffectiveMass = normalMass(first, second, point.armFirst, point.armSecond, normal);
                    if (normalEffectiveMass > 0) {
                        const float lambda = (point.targetVelocity - normalVelocity) / normalEffectiveMass;
                        const float previous = point.normalImpulse;
                        const float next = std::max(previous + lambda, 0.0F);
                        point.normalImpulse = next;
                        applyPairImpulse(
                            first, second, normal * (next - previous), point.armFirst, point.armSecond);
                    }
                }
            }
            // Трение ограничено импульсом нормали: тело скользит, пока сила трения
            // не превышает долю силы давления.
            const Vector2 tangent{-normal.y, normal.x};
            for (int index = 0; index < contact.count; ++index) {
                SolvePoint& point = contact.points[static_cast<std::size_t>(index)];
                const float tangentVelocity = dot(
                    velocityAt(second, point.armSecond) - velocityAt(first, point.armFirst), tangent);
                const float tangentEffectiveMass = normalMass(first, second, point.armFirst, point.armSecond, tangent);
                if (tangentEffectiveMass > 0) {
                    const float limit = contact.friction * point.normalImpulse;
                    const float previous = point.tangentImpulse;
                    const float next = std::clamp(
                        previous - tangentVelocity / tangentEffectiveMass, -limit, limit);
                    point.tangentImpulse = next;
                    applyPairImpulse(
                        first, second, tangent * (next - previous), point.armFirst, point.armSecond);
                }
            }
        }
    }
}

void PhysicsWorld::integratePositions(float deltaTime)
{
    const std::size_t count = slots.size();
    std::vector<Vector2> startPosition(count);
    std::vector<float> startAngle(count);
    std::vector<Vector2> endPosition(count);
    std::vector<float> endAngle(count);
    std::vector<bool> stopped(count, false);
    std::vector<CollisionShape> startShape(count);
    std::vector<Rect> path(count);
    for (std::size_t index = 0; index < count; ++index) {
        Slot& slot = slots[index];
        startPosition[index] = slot.body.position;
        startAngle[index] = slot.body.angle;
        endPosition[index] = slot.body.position;
        endAngle[index] = slot.body.angle;
        if (slot.removalPending) {
            continue;
        }
        if (slot.body.type != BodyType::Static) {
            endPosition[index] = slot.body.position + slot.body.velocity * deltaTime;
            endAngle[index] = slot.body.angle + slot.body.angularVelocity * deltaTime;
        }
        // Формы и пути нужны и у статических тел: они цели непрерывной проверки.
        startShape[index] = shapeAtPose(slot.body, startPosition[index], startAngle[index]);
        path[index] = pathBounds(startShape[index], shapeAtPose(slot.body, endPosition[index], endAngle[index]));
    }
    // Поза тела на долю шага: остановленное в точке удара тело стоит на месте.
    const auto poseAt = [&](std::size_t index, float fraction) -> std::pair<Vector2, float> {
        if (stopped[index]) {
            return {endPosition[index], endAngle[index]};
        }
        return {startPosition[index] + (endPosition[index] - startPosition[index]) * fraction,
            startAngle[index] + (endAngle[index] - startAngle[index]) * fraction};
    };
    // Первое касание пары на пути шага: выборка пути и уточнение границы бисекцией.
    const auto sweep = [&](std::size_t first, std::size_t second, float& fraction, Vector2& normal) {
        ContactManifold manifold;
        if (detectContact(startShape[first], startShape[second], manifold)) {
            // Уже касаются: этим занимается решатель контактов.
            return false;
        }
        const float travel = length((endPosition[first] - startPosition[first])
            - (endPosition[second] - startPosition[second]));
        const float extent = std::max(2.0F, 0.5F * (minimalWidth(slots[first].body.shape)
            + minimalWidth(slots[second].body.shape)));
        const int samples = static_cast<int>(std::min(96.0F, std::max(1.0F, std::ceil(travel / extent))));
        float previous = 0;
        float found = 1;
        bool hit = false;
        for (int step = 1; step <= samples; ++step) {
            const float stepFraction = static_cast<float>(step) / static_cast<float>(samples);
            const auto firstPose = poseAt(first, stepFraction);
            const auto secondPose = poseAt(second, stepFraction);
            if (detectContact(shapeAtPose(slots[first].body, firstPose.first, firstPose.second),
                    shapeAtPose(slots[second].body, secondPose.first, secondPose.second), manifold)) {
                previous = static_cast<float>(step - 1) / static_cast<float>(samples);
                found = stepFraction;
                hit = true;
                break;
            }
        }
        if (!hit) {
            return false;
        }
        float low = previous;
        float high = found;
        for (int step = 0; step < 12; ++step) {
            const float middle = (low + high) / 2;
            const auto firstPose = poseAt(first, middle);
            const auto secondPose = poseAt(second, middle);
            if (detectContact(shapeAtPose(slots[first].body, firstPose.first, firstPose.second),
                    shapeAtPose(slots[second].body, secondPose.first, secondPose.second), manifold)) {
                high = middle;
            } else {
                low = middle;
            }
        }
        fraction = high;
        const auto firstPose = poseAt(first, high);
        const auto secondPose = poseAt(second, high);
        if (detectContact(shapeAtPose(slots[first].body, firstPose.first, firstPose.second),
                shapeAtPose(slots[second].body, secondPose.first, secondPose.second), manifold)) {
            normal = -manifold.normal;
        } else {
            normal = Vector2{1, 0};
        }
        return true;
    };
    // Останавливаем ближайшие удары: ни одно тело не входит в другое на длину шага.
    std::set<std::pair<std::size_t, std::size_t>> resolved;
    const int passes = static_cast<int>(count) + 1;
    for (int pass = 0; pass < passes; ++pass) {
        float bestFraction = 2;
        std::size_t bestFirst = 0;
        std::size_t bestSecond = 0;
        Vector2 bestNormal;
        for (std::size_t first = 0; first < count; ++first) {
            RigidBody& firstBody = slots[first].body;
            if (firstBody.type != BodyType::Dynamic || firstBody.sleeping || stopped[first]
                || firstBody.sensor || slots[first].removalPending) {
                continue;
            }
            for (std::size_t second = 0; second < count; ++second) {
                if (second == first) {
                    continue;
                }
                const RigidBody& secondBody = slots[second].body;
                if (secondBody.sensor || slots[second].removalPending) {
                    continue;
                }
                if (!layersMatch(firstBody.layer, firstBody.mask, secondBody.layer, secondBody.mask)) {
                    continue;
                }
                const std::pair<std::size_t, std::size_t> key{
                    std::min(first, second), std::max(first, second)};
                if (resolved.count(key) != 0 || !intersects(path[first], path[second])) {
                    continue;
                }
                float fraction = 1;
                Vector2 normal;
                if (!sweep(first, second, fraction, normal) || fraction >= bestFraction) {
                    continue;
                }
                bestFraction = fraction;
                bestFirst = first;
                bestSecond = second;
                bestNormal = normal;
            }
        }
        if (bestFraction > 1) {
            break;
        }
        resolved.insert(std::pair<std::size_t, std::size_t>{std::min(bestFirst, bestSecond),
            std::max(bestFirst, bestSecond)});
        // Небольшая маржа внутрь препятствия: контакт получается устойчивым решателю,
        // а на экране такая глубина незаметна.
        const Vector2 relative = slots[bestFirst].body.velocity - slots[bestSecond].body.velocity;
        const float approach = -dot(relative, bestNormal);
        float fraction = bestFraction;
        if (approach > 1) {
            fraction = std::min(1.0F, fraction + ImpactMargin / (approach * deltaTime));
        }
        for (const std::size_t index : {bestFirst, bestSecond}) {
            if (stopped[index]) {
                continue;
            }
            // Задетое спящее тело просыпается: удар его разбудил.
            slots[index].body.sleeping = false;
            slots[index].body.sleepTime = 0;
            const auto pose = poseAt(index, fraction);
            startPosition[index] = pose.first;
            startAngle[index] = pose.second;
            endPosition[index] = pose.first;
            endAngle[index] = pose.second;
            stopped[index] = true;
        }
    }
    for (std::size_t index = 0; index < count; ++index) {
        if (slots[index].removalPending || slots[index].body.type == BodyType::Static
            || slots[index].body.sleeping) {
            continue;
        }
        slots[index].body.position = endPosition[index];
        slots[index].body.angle = endAngle[index];
    }
}

void PhysicsWorld::solvePositions()
{
    for (int iteration = 0; iteration < PositionIterations; ++iteration) {
        bool corrected = false;
        for (auto& entry : contacts) {
            SolveContact& contact = entry.second;
            if (contact.sensor || (!bodyIsActive(*contact.firstBody) && !bodyIsActive(*contact.secondBody))) {
                continue;
            }
            RigidBody& first = *contact.firstBody;
            RigidBody& second = *contact.secondBody;
            const float firstInverseInertia = first.inverseInertia();
            const float secondInverseInertia = second.inverseInertia();
            for (int index = 0; index < contact.count; ++index) {
                const SolvePoint& point = contact.points[static_cast<std::size_t>(index)];
                const Vector2 armFirst = rotated(point.localFirst, first.angle);
                const Vector2 armSecond = rotated(point.localSecond, second.angle);
                const float effectiveMass = normalMass(first, second, armFirst, armSecond, contact.manifold.normal);
                if (effectiveMass <= 0) {
                    continue;
                }
                const float correction = std::clamp(point.separation + LinearSlop, -MaximumCorrection, 0.0F);
                const float impulse = -PositionFactor * correction / effectiveMass;
                if (impulse <= 0) {
                    continue;
                }
                const Vector2 push = contact.manifold.normal * impulse;
                first.position -= push * first.inverseMass();
                first.angle -= cross(armFirst, push) * firstInverseInertia;
                second.position += push * second.inverseMass();
                second.angle += cross(armSecond, push) * secondInverseInertia;
                corrected = true;
            }
        }
        if (!corrected) {
            break;
        }
    }
}

void PhysicsWorld::resetForces()
{
    for (Slot& slot : slots) {
        slot.body.force = {};
        slot.body.torque = 0;
    }
}

void PhysicsWorld::emitEvents()
{
    std::map<std::pair<BodyId, BodyId>, PhysicsContact> current;
    for (const auto& entry : contacts) {
        const PhysicsContact contact = makeContact(entry.second);
        const ContactPhase phase = active.find(entry.first) == active.end()
            ? ContactPhase::Begin
            : ContactPhase::Persist;
        events.push_back(PhysicsContactEvent{phase, contact});
        current.emplace(entry.first, contact);
    }
    for (const auto& entry : active) {
        if (contacts.find(entry.first) == contacts.end()) {
            events.push_back(PhysicsContactEvent{ContactPhase::End, entry.second});
        }
    }
    active.swap(current);
}

void PhysicsWorld::eraseRemoved()
{
    const auto removed = std::remove_if(
        slots.begin(), slots.end(), [](const Slot& slot) { return slot.removalPending; });
    slots.erase(removed, slots.end());
}

PhysicsContact PhysicsWorld::makeContact(const SolveContact& contact) const
{
    PhysicsContact result;
    result.first = contact.first;
    result.second = contact.second;
    result.normal = contact.manifold.normal;
    result.depth = contact.manifold.depth;
    result.pointCount = contact.count;
    result.sensor = contact.sensor;
    for (int index = 0; index < contact.count; ++index) {
        const std::size_t position = static_cast<std::size_t>(index);
        result.points[position] = contact.manifold.points[position].position;
        if (!contact.sensor) {
            result.normalImpulse += contact.points[position].normalImpulse;
            result.tangentImpulse += contact.points[position].tangentImpulse;
        }
    }
    return result;
}
}
