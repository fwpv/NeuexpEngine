#include "world.h"
#include "scene/scene.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <utility>

namespace NeuexpEngine
{
namespace
{
// Ограниченное число итераций решателя: скорость тел стабилизируется за несколько
// проходов, а стоимость шага остаётся предсказуемой.
constexpr int VelocityIterations = 8;
constexpr int PositionIterations = 3;
// Доля устранённого проникновения за итерацию и запас, который проникновением не считается.
constexpr float PositionFactor = 0.2F;
constexpr float LinearSlop = 0.1F;
constexpr float MaximumCorrection = 4.0F;
// Ниже этой скорости сближения удар не возвращает энергию: иначе покоящееся тело дрожит.
constexpr float RestitutionThreshold = 20.0F;
// Небольшое проникновение в точке удара: контакт получается устойчивым, как у обычного
// решателя, а на экране такая глубина незаметна.
constexpr float ImpactMargin = 0.2F;
// Квадрат допустимого сдвига локальных якорей, при котором точка контакта считается
// прежней и получает импульсы прошлого шага.
constexpr float WarmStartTolerance = 4.0F;

Vector2 velocityAt(const RigidBody& body, Vector2 arm)
{
    return body.velocity + Vector2{-body.angularVelocity * arm.y, body.angularVelocity * arm.x};
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

bool poseOverlaps(const RigidBody& body, Vector2 position, float angle, const CollisionShape& target)
{
    ContactManifold manifold;
    return detectContact(shapeAtPose(body, position, angle), target, manifold);
}
}

void PhysicsWorld::clear()
{
    slots.clear();
    contacts.clear();
    active.clear();
    warmContacts.clear();
    events.clear();
}

BodyId PhysicsWorld::addBody(const RigidBody& description)
{
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
    solveVelocities();
    integratePositions(deltaTime);
    solvePositions();
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
        if (body.type != BodyType::Dynamic) {
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
    if ((bodyA.layer & bodyB.mask) == 0 || (bodyB.layer & bodyA.mask) == 0) {
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
    for (int index = 0; index < manifold.count; ++index) {
        SolvePoint& point = contact.points[static_cast<std::size_t>(index)];
        point.separation = -manifold.points[static_cast<std::size_t>(index)].depth;
        point.armFirst = manifold.points[static_cast<std::size_t>(index)].position - contact.firstBody->position;
        point.armSecond = manifold.points[static_cast<std::size_t>(index)].position - contact.secondBody->position;
        point.localFirst = rotated(point.armFirst, -contact.firstBody->angle);
        point.localSecond = rotated(point.armSecond, -contact.secondBody->angle);
        const Vector2 relative = velocityAt(*contact.secondBody, point.armSecond)
            - velocityAt(*contact.firstBody, point.armFirst);
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
    // Импульсы прошлого шага переносятся только точкам с теми же локальными якорями:
    // точка касания на теле устойчива между шагами, а новая грань начинает с нуля.
    const std::pair<BodyId, BodyId> key{contact.first, contact.second};
    const auto previous = warmContacts.find(key);
    if (previous == warmContacts.end()) {
        return;
    }
    std::array<bool, 2> used = {false, false};
    for (int index = 0; index < contact.count; ++index) {
        SolvePoint& point = contact.points[static_cast<std::size_t>(index)];
        float bestDistance = WarmStartTolerance;
        int best = -1;
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
        if (contact.sensor) {
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
        if (contact.sensor) {
            continue;
        }
        WarmContact warm;
        warm.count = contact.count;
        for (int index = 0; index < contact.count; ++index) {
            const SolvePoint& point = contact.points[static_cast<std::size_t>(index)];
            WarmPoint& stored = warm.points[static_cast<std::size_t>(index)];
            stored.localFirst = point.localFirst;
            stored.localSecond = point.localSecond;
            stored.normalImpulse = point.normalImpulse;
            stored.tangentImpulse = point.tangentImpulse;
        }
        warmContacts.emplace(entry.first, warm);
    }
}

void PhysicsWorld::solveVelocities()
{
    applyWarmStart();
    for (int iteration = 0; iteration < VelocityIterations; ++iteration) {
        for (auto& entry : contacts) {
            SolveContact& contact = entry.second;
            if (contact.sensor) {
                continue;
            }
            RigidBody& first = *contact.firstBody;
            RigidBody& second = *contact.secondBody;
            for (int index = 0; index < contact.count; ++index) {
                SolvePoint& point = contact.points[static_cast<std::size_t>(index)];
                const Vector2 normal = contact.manifold.normal;
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

                // Трение ограничено импульсом нормали: тело скользит, пока сила трения
                // не превышает долю силы давления.
                const Vector2 tangent{-normal.y, normal.x};
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

float PhysicsWorld::timeOfImpact(const RigidBody& body, Vector2 startPosition, float startAngle,
    Vector2 endPosition, float endAngle, Vector2& normal) const
{
    const CollisionShape startShape = shapeAtPose(body, startPosition, startAngle);
    const Rect path = pathBounds(startShape, shapeAtPose(body, endPosition, endAngle));
    const float travel = length(endPosition - startPosition);
    float impact = 1;
    for (const BroadEntry& target : entries) {
        if (target.body == &body || target.body->type != BodyType::Static || target.body->sensor) {
            continue;
        }
        if ((body.layer & target.body->mask) == 0 || (target.body->layer & body.mask) == 0) {
            continue;
        }
        if (!intersects(path, target.bounds)) {
            continue;
        }
        ContactManifold manifold;
        // Уже касается в начале шага: этим занимается решатель контактов.
        if (detectContact(startShape, target.shape, manifold)) {
            continue;
        }
        // Путь просматривается выборкой: препятствие может быть пройдено целиком,
        // и проверки одного конечного положения мало.
        const float extent = std::max(2.0F, 0.5F * (minimalWidth(body.shape) + minimalWidth(target.shape)));
        const int samples = static_cast<int>(std::min(48.0F, std::max(1.0F, std::ceil(travel / extent))));
        float previous = 0;
        float found = 1;
        bool hit = false;
        for (int index = 1; index <= samples; ++index) {
            const float fraction = static_cast<float>(index) / static_cast<float>(samples);
            if (poseOverlaps(body, startPosition + (endPosition - startPosition) * fraction,
                    startAngle + (endAngle - startAngle) * fraction, target.shape)) {
                previous = static_cast<float>(index - 1) / static_cast<float>(samples);
                found = fraction;
                hit = true;
                break;
            }
        }
        if (!hit) {
            continue;
        }
        // Уточнение первого касания между двумя соседними выборками.
        float low = previous;
        float high = found;
        for (int step = 0; step < 12; ++step) {
            const float middle = (low + high) / 2;
            if (poseOverlaps(body, startPosition + (endPosition - startPosition) * middle,
                    startAngle + (endAngle - startAngle) * middle, target.shape)) {
                high = middle;
            } else {
                low = middle;
            }
        }
        if (high < impact) {
            impact = high;
            ContactManifold manifold;
            if (detectContact(shapeAtPose(body, startPosition + (endPosition - startPosition) * high,
                    startAngle + (endAngle - startAngle) * high), target.shape, manifold)) {
                normal = -manifold.normal;
            }
        }
    }
    return impact;
}

void PhysicsWorld::integratePositions(float deltaTime)
{
    for (Slot& slot : slots) {
        if (slot.removalPending || slot.body.type == BodyType::Static) {
            continue;
        }
        RigidBody& body = slot.body;
        const Vector2 startPosition = body.position;
        const float startAngle = body.angle;
        const Vector2 endPosition = startPosition + body.velocity * deltaTime;
        const float endAngle = startAngle + body.angularVelocity * deltaTime;
        if (body.type == BodyType::Dynamic) {
            Vector2 normal;
            const float impact = timeOfImpact(body, startPosition, startAngle, endPosition, endAngle, normal);
            if (impact < 1) {
                // Тело останавливается в точке касания: за шаг оно не заходит внутрь
                // препятствия. Остаток пути внутри препятствия — только небольшая доля
                // маржи, чтобы контакт оставался устойчивым решателю.
                float fraction = impact;
                const float approach = -dot(body.velocity, normal);
                if (approach > 1) {
                    fraction = std::min(1.0F, fraction + ImpactMargin / (approach * deltaTime));
                }
                body.position = startPosition + (endPosition - startPosition) * fraction;
                body.angle = startAngle + (endAngle - startAngle) * fraction;
                continue;
            }
        }
        body.position = endPosition;
        body.angle = endAngle;
    }
}

void PhysicsWorld::solvePositions()
{
    for (int iteration = 0; iteration < PositionIterations; ++iteration) {
        for (auto& entry : contacts) {
            SolveContact& contact = entry.second;
            if (contact.sensor) {
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
            }
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
        events.push_back({phase, contact});
        current.emplace(entry.first, contact);
    }
    for (const auto& entry : active) {
        if (contacts.find(entry.first) == contacts.end()) {
            events.push_back({ContactPhase::End, entry.second});
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
