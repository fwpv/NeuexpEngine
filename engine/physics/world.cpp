#include "world.h"
#include "body.h"
#include "scene/object.h"

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

Vector2 velocityAtPoint(Vector2 linearVelocity, float angularVelocity, Vector2 arm)
{
    return linearVelocity + Vector2{-angularVelocity * arm.y, angularVelocity * arm.x};
}

// Мировая форма объекта в заданной позе: масштаб и origin берутся из трансформации,
// меняются только положение и угол. Используется для поиска времени удара.
CollisionShape shapeAtPose(const GameObject& object, Vector2 position, float angle)
{
    Transform2 pose = object.transform();
    pose.position = position;
    pose.rotation = angle;
    return object.collider().worldShape(pose);
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

bool PhysicsWorld::bodyIsActive(const Body& body)
{
    const PhysicsBody& state = body.object->body();
    if (state.type == BodyType::Dynamic) {
        return !state.sleeping;
    }
    if (state.type == BodyType::Kinematic) {
        return state.velocity.x != 0 || state.velocity.y != 0 || state.angularVelocity != 0
            || body.transformMoved;
    }
    return false;
}

Vector2 PhysicsWorld::velocityAt(const Body& body, Vector2 arm)
{
    return velocityAtPoint(body.object->body().velocity,
        body.object->body().angularVelocity, arm);
}

void PhysicsWorld::applyPairImpulse(Body& first, Body& second, Vector2 impulse, Vector2 armFirst,
    Vector2 armSecond)
{
    PhysicsBody& firstState = first.object->body();
    firstState.velocity -= impulse * first.inverseMass;
    firstState.angularVelocity -= cross(armFirst, impulse) * first.inverseInertia;
    PhysicsBody& secondState = second.object->body();
    secondState.velocity += impulse * second.inverseMass;
    secondState.angularVelocity += cross(armSecond, impulse) * second.inverseInertia;
}

void PhysicsWorld::clear()
{
    contacts.clear();
    active.clear();
    warmContacts.clear();
    previousPoses.clear();
    events.clear();
}

void PhysicsWorld::step(const std::vector<GameObject*>& objects, float deltaTime)
{
    assert(deltaTime > 0 && std::isfinite(deltaTime));
    events.clear();
    collectBodies(objects);
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
    // Тела и контакты нужны только на время шага: между шагами мир держит только
    // идентификаторы и позы, поэтому удалённый после шага объект не оставляет
    // висячих указателей.
    bodies.clear();
    order.clear();
    contacts.clear();
}

void PhysicsWorld::collectBodies(const std::vector<GameObject*>& objects)
{
    bodies.clear();
    for (GameObject* object : objects) {
        // Объект участвует, если у него включён коллайдер; без тела он неподвижен.
        if (!object->collider().enabled) {
            continue;
        }
        const Transform2 pose = object->transform();
        // Параметры тел и поверхностей проверяются в отладочной сборке: отрицательное
        // трение или упругость вне [0, 1] накачивают энергию, а не конечные значения
        // дают NaN в решателе.
        [[maybe_unused]] const PhysicsBody& body = object->body();
        [[maybe_unused]] const Collider& collider = object->collider();
        assert(std::isfinite(pose.position.x) && std::isfinite(pose.position.y));
        assert(std::isfinite(pose.rotation) && std::isfinite(pose.scale.x) && std::isfinite(pose.scale.y)
            && std::isfinite(pose.origin.x) && std::isfinite(pose.origin.y));
        assert(std::isfinite(collider.friction) && collider.friction >= 0 && collider.friction <= 1);
        assert(std::isfinite(collider.restitution) && collider.restitution >= 0
            && collider.restitution <= 1);
        if (body.type != BodyType::Static) {
            assert(std::isfinite(body.density) && body.density > 0);
            assert(std::isfinite(body.linearDamping) && body.linearDamping >= 0);
            assert(std::isfinite(body.angularDamping) && body.angularDamping >= 0);
            assert(std::isfinite(body.gravityScale));
            assert(std::isfinite(body.velocity.x) && std::isfinite(body.velocity.y)
                && std::isfinite(body.angularVelocity));
        }
        Body entry;
        entry.object = object;
        entry.id = object->id();
        const auto previousPose = previousPoses.find(entry.id);
        // Нет прошлой позы — новое тело: считаем его перенесённым, чтобы оно сразу
        // участвовало в пробуждении, как и тело, чью позу изменили между шагами.
        entry.transformMoved = object->body().type == BodyType::Kinematic
            && (previousPose == previousPoses.end() || previousPose->second.position != pose.position
                || previousPose->second.rotation != pose.rotation);
        previousPoses[entry.id] = pose;
        entry.shape = object->collider().worldShape(pose);
        entry.bounds = bounds(entry.shape);
        entry.previousVelocity = object->body().velocity;
        entry.previousAngularVelocity = object->body().angularVelocity;
        const BodyMass mass = bodyMassData(entry.shape, object->transform().position,
            object->body().density);
        entry.inverseMass = object->body().type == BodyType::Dynamic && mass.mass > 0
            ? 1 / mass.mass : 0;
        entry.inverseInertia = object->body().type == BodyType::Dynamic && mass.inertia > 0
            ? 1 / mass.inertia : 0;
        bodies.push_back(entry);
    }
    for (auto pose = previousPoses.begin(); pose != previousPoses.end();) {
        const bool alive = std::any_of(bodies.begin(), bodies.end(),
            [&pose](const Body& entry) { return entry.id == pose->first; });
        if (alive) {
            ++pose;
        } else {
            pose = previousPoses.erase(pose);
        }
    }
}

void PhysicsWorld::integrateVelocities(float deltaTime)
{
    for (Body& entry : bodies) {
        GameObject& object = *entry.object;
        PhysicsBody& body = object.body();
        if (body.type != BodyType::Dynamic) {
            continue;
        }
        if (body.sleeping && (body.force.x != 0 || body.force.y != 0 || body.torque != 0
                || body.velocity.x != 0 || body.velocity.y != 0 || body.angularVelocity != 0)) {
            body.sleeping = false;
            body.sleepTime = 0;
        }
        if (body.sleeping) {
            continue;
        }
        body.velocity += (gravity * body.gravityScale + body.force * entry.inverseMass) * deltaTime;
        body.angularVelocity += body.torque * entry.inverseInertia * deltaTime;
        body.velocity *= 1 / (1 + body.linearDamping * deltaTime);
        body.angularVelocity /= 1 + body.angularDamping * deltaTime;
    }
}

void PhysicsWorld::collectContacts()
{
    contacts.clear();
    order.resize(bodies.size());
    for (std::size_t index = 0; index < order.size(); ++index) {
        order[index] = index;
    }
    std::sort(order.begin(), order.end(), [this](std::size_t first, std::size_t second) {
        const Body& firstEntry = bodies[first];
        const Body& secondEntry = bodies[second];
        if (firstEntry.bounds.position.x != secondEntry.bounds.position.x) {
            return firstEntry.bounds.position.x < secondEntry.bounds.position.x;
        }
        return firstEntry.id < secondEntry.id;
    });
    for (std::size_t first = 0; first < order.size(); ++first) {
        const Body& entry = bodies[order[first]];
        const float right = entry.bounds.position.x + entry.bounds.size.width;
        for (std::size_t second = first + 1; second < order.size(); ++second) {
            const Body& other = bodies[order[second]];
            if (other.bounds.position.x > right) {
                break;
            }
            if (!intersects(entry.bounds, other.bounds)) {
                continue;
            }
            checkPair(order[first], order[second]);
        }
    }
}

void PhysicsWorld::checkPair(std::size_t firstIndex, std::size_t secondIndex)
{
    Body& entryA = bodies[firstIndex];
    Body& entryB = bodies[secondIndex];
    GameObject& objectA = *entryA.object;
    GameObject& objectB = *entryB.object;
    // Пара из двух неподвижных объектов не проверяется: события и контакты есть
    // у пар, где участвует динамическое или кинематическое тело.
    if (objectA.body().type == BodyType::Static && objectB.body().type == BodyType::Static) {
        return;
    }
    if (!layersMatch(objectA.collider().layer, objectA.collider().mask,
            objectB.collider().layer, objectB.collider().mask)) {
        return;
    }
    const bool firstIsSmaller = entryA.id < entryB.id;
    const CollisionShape& firstShape = firstIsSmaller ? entryA.shape : entryB.shape;
    const CollisionShape& secondShape = firstIsSmaller ? entryB.shape : entryA.shape;
    ContactManifold manifold;
    if (!detectContact(firstShape, secondShape, manifold)) {
        return;
    }
    SolveContact contact;
    contact.first = firstIsSmaller ? entryA.id : entryB.id;
    contact.second = firstIsSmaller ? entryB.id : entryA.id;
    contact.firstBody = firstIsSmaller ? &entryA : &entryB;
    contact.secondBody = firstIsSmaller ? &entryB : &entryA;
    contact.manifold = manifold;
    contact.sensor = objectA.collider().trigger || objectB.collider().trigger;
    contact.friction = std::sqrt(objectA.collider().friction * objectB.collider().friction);
    const float restitution = std::max(objectA.collider().restitution, objectB.collider().restitution);
    contact.count = manifold.count;
    const Vector2 firstPreviousVelocity = firstIsSmaller ? entryA.previousVelocity : entryB.previousVelocity;
    const float firstPreviousAngular = firstIsSmaller
        ? entryA.previousAngularVelocity : entryB.previousAngularVelocity;
    const Vector2 secondPreviousVelocity = firstIsSmaller
        ? entryB.previousVelocity : entryA.previousVelocity;
    const float secondPreviousAngular = firstIsSmaller
        ? entryB.previousAngularVelocity : entryA.previousAngularVelocity;
    for (int index = 0; index < manifold.count; ++index) {
        SolvePoint& point = contact.points[static_cast<std::size_t>(index)];
        const ContactPoint& contactPoint = manifold.points[static_cast<std::size_t>(index)];
        point.separation = -contactPoint.depth;
        point.armFirst = contactPoint.position - contact.firstBody->object->transform().position;
        point.armSecond = contactPoint.position - contact.secondBody->object->transform().position;
        point.localFirst = rotated(point.armFirst, -contact.firstBody->object->transform().rotation);
        point.localSecond = rotated(point.armSecond, -contact.secondBody->object->transform().rotation);
        point.id = contactPoint.id;
        // Упругость считается по скорости до шага: иначе к удару добавляется g * dT
        // и тело с упругостью 1 постепенно набирает высоту.
        const Vector2 relative = velocityAtPoint(secondPreviousVelocity, secondPreviousAngular, point.armSecond)
            - velocityAtPoint(firstPreviousVelocity, firstPreviousAngular, point.armFirst);
        const float normalVelocity = dot(relative, manifold.normal);
        point.targetVelocity = normalVelocity < -RestitutionThreshold ? -restitution * normalVelocity : 0;
    }
    const std::pair<ObjectId, ObjectId> key{contact.first, contact.second};
    if (active.find(key) != active.end()) {
        warmStartContact(contact);
    }
    contacts.emplace(key, contact);
}

void PhysicsWorld::warmStartContact(SolveContact& contact)
{
    // Импульсы прошлого шага переносятся только точкам с той же особенностью форм:
    // новая грань или вершина начинает с нуля, а устойчивый контакт сохраняет импульсы.
    const std::pair<ObjectId, ObjectId> key{contact.first, contact.second};
    const auto previous = warmContacts.find(key);
    if (previous == warmContacts.end()) {
        return;
    }
    std::array<bool, 2> used = {false, false};
    for (int index = 0; index < contact.count; ++index) {
        SolvePoint& point = contact.points[static_cast<std::size_t>(index)];
        int best = -1;
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
        const Vector2 normal = contact.manifold.normal;
        const Vector2 tangent{-normal.y, normal.x};
        for (int index = 0; index < contact.count; ++index) {
            const SolvePoint& point = contact.points[static_cast<std::size_t>(index)];
            applyPairImpulse(*contact.firstBody, *contact.secondBody,
                normal * point.normalImpulse + tangent * point.tangentImpulse,
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
            PhysicsBody& first = contact.firstBody->object->body();
            PhysicsBody& second = contact.secondBody->object->body();
            if (bodyIsActive(*contact.firstBody) && second.sleeping) {
                second.sleeping = false;
                second.sleepTime = 0;
                changed = true;
            }
            if (bodyIsActive(*contact.secondBody) && first.sleeping) {
                first.sleeping = false;
                first.sleepTime = 0;
                changed = true;
            }
        }
    }
}

void PhysicsWorld::updateSleeping(float deltaTime)
{
    for (Body& entry : bodies) {
        PhysicsBody& body = entry.object->body();
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
    const std::size_t count = bodies.size();
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
    for (const auto& entry : contacts) {
        const SolveContact& contact = entry.second;
        if (contact.sensor || contact.firstBody->object->body().type != BodyType::Dynamic
            || contact.secondBody->object->body().type != BodyType::Dynamic) {
            continue;
        }
        const std::size_t firstIndex = static_cast<std::size_t>(contact.firstBody - bodies.data());
        const std::size_t secondIndex = static_cast<std::size_t>(contact.secondBody - bodies.data());
        if (firstIndex < count && secondIndex < count) {
            unite(firstIndex, secondIndex);
        }
    }
    std::vector<bool> ready(count, false);
    for (std::size_t index = 0; index < count; ++index) {
        const PhysicsBody& body = bodies[index].object->body();
        if (body.type != BodyType::Dynamic || body.sleeping || body.sleepTime < SleepTime) {
            continue;
        }
        ready[index] = true;
    }
    for (std::size_t index = 0; index < count; ++index) {
        if (!ready[index]) {
            continue;
        }
        bool islandReady = true;
        for (std::size_t other = 0; other < count; ++other) {
            if (root(other) != root(index)) {
                continue;
            }
            const PhysicsBody& body = bodies[other].object->body();
            if (body.type != BodyType::Dynamic || body.sleeping) {
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
            PhysicsBody& body = bodies[other].object->body();
            if (body.type != BodyType::Dynamic || body.sleeping) {
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
    Body& first = *contact.firstBody;
    Body& second = *contact.secondBody;
    const Vector2 normal = contact.manifold.normal;
    const float inverseMass = first.inverseMass + second.inverseMass;
    const float inverseInertiaFirst = first.inverseInertia;
    const float inverseInertiaSecond = second.inverseInertia;
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
            if (contact.sensor
                || (!bodyIsActive(*contact.firstBody) && !bodyIsActive(*contact.secondBody))) {
                continue;
            }
            Body& first = *contact.firstBody;
            Body& second = *contact.secondBody;
            const Vector2 normal = contact.manifold.normal;
            if (!(contact.count == 2 && solveNormalBlock(contact))) {
                for (int index = 0; index < contact.count; ++index) {
                    SolvePoint& point = contact.points[static_cast<std::size_t>(index)];
                    const float normalVelocity = dot(
                        velocityAt(second, point.armSecond) - velocityAt(first, point.armFirst), normal);
                    const float normalEffectiveMass = first.inverseMass + second.inverseMass
                        + first.inverseInertia * cross(point.armFirst, normal) * cross(point.armFirst, normal)
                        + second.inverseInertia * cross(point.armSecond, normal) * cross(point.armSecond, normal);
                    if (normalEffectiveMass > 0) {
                        const float lambda = (point.targetVelocity - normalVelocity) / normalEffectiveMass;
                        const float previous = point.normalImpulse;
                        const float next = std::max(previous + lambda, 0.0F);
                        point.normalImpulse = next;
                        applyPairImpulse(first, second, normal * (next - previous),
                            point.armFirst, point.armSecond);
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
                const float tangentEffectiveMass = first.inverseMass + second.inverseMass
                    + first.inverseInertia * cross(point.armFirst, tangent) * cross(point.armFirst, tangent)
                    + second.inverseInertia * cross(point.armSecond, tangent) * cross(point.armSecond, tangent);
                if (tangentEffectiveMass > 0) {
                    const float limit = contact.friction * point.normalImpulse;
                    const float previous = point.tangentImpulse;
                    const float next = std::clamp(
                        previous - tangentVelocity / tangentEffectiveMass, -limit, limit);
                    point.tangentImpulse = next;
                    applyPairImpulse(first, second, tangent * (next - previous),
                        point.armFirst, point.armSecond);
                }
            }
        }
    }
}

void PhysicsWorld::integratePositions(float deltaTime)
{
    const std::size_t count = bodies.size();
    std::vector<Vector2> startPosition(count);
    std::vector<float> startAngle(count);
    std::vector<Vector2> endPosition(count);
    std::vector<float> endAngle(count);
    std::vector<bool> stopped(count, false);
    std::vector<CollisionShape> startShape(count);
    std::vector<Rect> path(count);
    for (std::size_t index = 0; index < count; ++index) {
        Body& entry = bodies[index];
        GameObject& object = *entry.object;
        startPosition[index] = object.transform().position;
        startAngle[index] = object.transform().rotation;
        endPosition[index] = startPosition[index];
        endAngle[index] = startAngle[index];
        if (object.body().type != BodyType::Static && !object.body().sleeping) {
            endPosition[index] = startPosition[index] + object.body().velocity * deltaTime;
            endAngle[index] = startAngle[index] + object.body().angularVelocity * deltaTime;
        }
        // Формы и пути нужны и у статических тел: они цели непрерывной проверки.
        startShape[index] = shapeAtPose(object, startPosition[index], startAngle[index]);
        path[index] = pathBounds(startShape[index],
            shapeAtPose(object, endPosition[index], endAngle[index]));
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
        const float extent = std::max(2.0F, 0.5F * (minimalWidth(bodies[first].shape)
            + minimalWidth(bodies[second].shape)));
        const int samples = static_cast<int>(std::min(96.0F, std::max(1.0F, std::ceil(travel / extent))));
        float previous = 0;
        float found = 1;
        bool hit = false;
        for (int step = 1; step <= samples; ++step) {
            const float stepFraction = static_cast<float>(step) / static_cast<float>(samples);
            const auto firstPose = poseAt(first, stepFraction);
            const auto secondPose = poseAt(second, stepFraction);
            if (detectContact(shapeAtPose(*bodies[first].object, firstPose.first, firstPose.second),
                    shapeAtPose(*bodies[second].object, secondPose.first, secondPose.second), manifold)) {
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
            if (detectContact(shapeAtPose(*bodies[first].object, firstPose.first, firstPose.second),
                    shapeAtPose(*bodies[second].object, secondPose.first, secondPose.second), manifold)) {
                high = middle;
            } else {
                low = middle;
            }
        }
        fraction = high;
        const auto firstPose = poseAt(first, high);
        const auto secondPose = poseAt(second, high);
        if (detectContact(shapeAtPose(*bodies[first].object, firstPose.first, firstPose.second),
                shapeAtPose(*bodies[second].object, secondPose.first, secondPose.second), manifold)) {
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
            GameObject& firstObject = *bodies[first].object;
            if (firstObject.body().type != BodyType::Dynamic || firstObject.body().sleeping
                || stopped[first] || firstObject.collider().trigger) {
                continue;
            }
            for (std::size_t second = 0; second < count; ++second) {
                if (second == first) {
                    continue;
                }
                GameObject& secondObject = *bodies[second].object;
                if (secondObject.collider().trigger) {
                    continue;
                }
                if (!layersMatch(firstObject.collider().layer, firstObject.collider().mask,
                        secondObject.collider().layer, secondObject.collider().mask)) {
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
        const Vector2 relative = bodies[bestFirst].object->body().velocity
            - bodies[bestSecond].object->body().velocity;
        const float approach = -dot(relative, bestNormal);
        float fraction = bestFraction;
        if (approach > 1) {
            fraction = std::min(1.0F, fraction + ImpactMargin / (approach * deltaTime));
        }
        for (const std::size_t index : {bestFirst, bestSecond}) {
            if (stopped[index]) {
                continue;
            }
            PhysicsBody& body = bodies[index].object->body();
            body.sleeping = false;
            body.sleepTime = 0;
            const auto pose = poseAt(index, fraction);
            startPosition[index] = pose.first;
            startAngle[index] = pose.second;
            endPosition[index] = pose.first;
            endAngle[index] = pose.second;
            stopped[index] = true;
        }
    }
    for (std::size_t index = 0; index < count; ++index) {
        GameObject& object = *bodies[index].object;
        if (object.body().type == BodyType::Static || object.body().sleeping) {
            continue;
        }
        object.transform().position = endPosition[index];
        object.transform().rotation = endAngle[index];
    }
}

void PhysicsWorld::solvePositions()
{
    for (int iteration = 0; iteration < PositionIterations; ++iteration) {
        bool corrected = false;
        for (auto& entry : contacts) {
            SolveContact& contact = entry.second;
            if (contact.sensor
                || (!bodyIsActive(*contact.firstBody) && !bodyIsActive(*contact.secondBody))) {
                continue;
            }
            Body& first = *contact.firstBody;
            Body& second = *contact.secondBody;
            GameObject& firstObject = *first.object;
            GameObject& secondObject = *second.object;
            const Vector2 normal = contact.manifold.normal;
            for (int index = 0; index < contact.count; ++index) {
                const SolvePoint& point = contact.points[static_cast<std::size_t>(index)];
                const Vector2 armFirst = rotated(point.localFirst, firstObject.transform().rotation);
                const Vector2 armSecond = rotated(point.localSecond, secondObject.transform().rotation);
                const float effectiveMass = first.inverseMass + second.inverseMass
                    + first.inverseInertia * cross(armFirst, normal) * cross(armFirst, normal)
                    + second.inverseInertia * cross(armSecond, normal) * cross(armSecond, normal);
                if (effectiveMass <= 0) {
                    continue;
                }
                const float correction = std::clamp(point.separation + LinearSlop, -MaximumCorrection, 0.0F);
                const float impulse = -PositionFactor * correction / effectiveMass;
                if (impulse <= 0) {
                    continue;
                }
                const Vector2 push = normal * impulse;
                firstObject.transform().position -= push * first.inverseMass;
                firstObject.transform().rotation -= cross(armFirst, push) * first.inverseInertia;
                secondObject.transform().position += push * second.inverseMass;
                secondObject.transform().rotation += cross(armSecond, push) * second.inverseInertia;
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
    for (Body& entry : bodies) {
        entry.object->body().force = {};
        entry.object->body().torque = 0;
    }
}

void PhysicsWorld::emitEvents()
{
    std::map<std::pair<ObjectId, ObjectId>, PhysicsContact> current;
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
