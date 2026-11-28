#include "collision/manifold.h"
#include "collision/query.h"
#include "core/geometry.h"
#include "core/math2d.h"
#include "physics/body.h"
#include "physics/world.h"
#include "scene/scene.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace NeuexpEngine;

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool closeTo(float first, float second, float tolerance)
{
    return std::fabs(first - second) <= tolerance;
}

constexpr float StepTime = 1.0F / 60;

PhysicsBody dynamicBody()
{
    PhysicsBody body;
    body.type = BodyType::Dynamic;
    return body;
}

// Сцена с физикой: объект получает коллайдер как форму и тело как динамику.
class TestWorld
{
public:
    Scene& scene() { return owned; }
    PhysicsWorld& physics() { return owned.physics(); }
    const std::vector<PhysicsContactEvent>& events() const { return owned.physics().contactEvents(); }

    GameObject& body(ObjectId id)
    {
        GameObject* object = owned.find(id);
        require(object != nullptr, "Body object is missing.");
        return *object;
    }

    ObjectId addCircle(Vector2 position, float radius, BodyType type = BodyType::Dynamic)
    {
        return addShape(Circle{{0, 0}, radius}, position, 0, type);
    }

    ObjectId addBox(Vector2 position, Size2 size, BodyType type = BodyType::Dynamic, float angle = 0)
    {
        return addShape(Box{{0, 0}, size, 0}, position, angle, type);
    }

    // Неподвижная геометрия без тела: коллайдер сам участвует как статика.
    ObjectId addStatic(const CollisionShape& shape, Vector2 position)
    {
        auto object = std::make_unique<GameObject>();
        object->collider() = Collider{shape};
        object->transform().position = position;
        return addObject(std::move(object));
    }

    // Неподвижный пол шириной width с верхней гранью на y = top.
    ObjectId addFloor(float top, float width = 600)
    {
        const ObjectId id = addStatic(Box{{0, 0}, {width, 40}, 0}, {0, top + 20});
        body(id).collider().friction = 1;
        return id;
    }

    ObjectId addShape(const CollisionShape& shape, Vector2 position, float angle, BodyType type)
    {
        auto object = std::make_unique<GameObject>();
        object->collider() = Collider{shape};
        object->transform().position = position;
        object->transform().rotation = angle;
        PhysicsBody settings;
        settings.type = type;
        object->enablePhysics(settings);
        return addObject(std::move(object));
    }

    ObjectId addObject(std::unique_ptr<GameObject> object) { return owned.add(std::move(object)); }

    void step() { owned.update(StepTime); }
    void steps(int count)
    {
        for (int index = 0; index < count; ++index) {
            owned.update(StepTime);
        }
    }

private:
    Scene owned;
};

int countPhase(const std::vector<PhysicsContactEvent>& events, ContactPhase phase)
{
    return static_cast<int>(std::count_if(events.begin(), events.end(),
        [phase](const PhysicsContactEvent& event) { return event.phase == phase; }));
}

void testManifold()
{
    ContactManifold manifold;

    const CollisionShape firstCircle{Circle{{0, 0}, 5}};
    const CollisionShape secondCircle{Circle{{6, 0}, 5}};
    require(detectContact(firstCircle, secondCircle, manifold), "Circles must touch.");
    require(manifold.count == 1 && closeTo(manifold.depth, 4, 1e-4F), "Circle contact depth failed.");
    require(closeTo(manifold.normal.x, 1, 1e-5F) && closeTo(manifold.points[0].position.x, 3, 1e-4F),
        "Circle contact normal or point failed.");
    require(!detectContact(firstCircle, CollisionShape{Circle{{20, 0}, 5}}, manifold),
        "Separated circles must not touch.");

    // Грань к грани: две контактные точки на общей грани.
    const CollisionShape firstBox{Box{{0, 0}, {10, 10}, 0}};
    require(detectContact(firstBox, CollisionShape{Box{{8, 0}, {10, 10}, 0}}, manifold), "Boxes must touch.");
    require(manifold.count == 2 && closeTo(manifold.depth, 2, 1e-4F), "Face contact must have two points.");
    require(closeTo(manifold.normal.x, 1, 1e-5F), "Face contact normal must point to the second box.");

    // Нормаль разворачивается при обратном порядке форм.
    require(detectContact(CollisionShape{Box{{8, 0}, {10, 10}, 0}}, firstBox, manifold),
        "Reversed boxes must touch.");
    require(closeTo(manifold.normal.x, -1, 1e-5F), "Reversed contact normal must be flipped.");

    // Круг перед прямоугольником: точка на ближайшей грани.
    require(detectContact(CollisionShape{Circle{{7, 0}, 3}}, firstBox, manifold), "Circle must touch the box.");
    require(closeTo(manifold.depth, 1, 1e-4F) && closeTo(manifold.normal.x, -1, 1e-5F)
            && manifold.count == 1,
        "Circle-box contact failed.");
    require(closeTo(manifold.points[0].position.x, 5, 1e-4F), "Circle-box contact point must lie on the face.");

    // Пересечение сцены и контакт физики считаются одним проходом: решения совпадают.
    Manifold overlap;
    require(detectContact(firstCircle, secondCircle, manifold) && collide(firstCircle, secondCircle, overlap),
        "Overlap must find the same circle pair.");
    require(overlap.normal == manifold.normal && closeTo(overlap.depth, manifold.depth, 1e-5F),
        "Circle overlap and contact must return the same manifold.");
    require(detectContact(firstBox, CollisionShape{Box{{8, 0}, {10, 10}, 0}}, manifold)
            && collide(firstBox, CollisionShape{Box{{8, 0}, {10, 10}, 0}}, overlap),
        "Overlap must find the same box pair.");
    require(overlap.normal == manifold.normal && closeTo(overlap.depth, manifold.depth, 1e-5F),
        "Box overlap and contact must return the same manifold.");
    require(detectContact(firstBox, CollisionShape{Circle{{7, 0}, 3}}, manifold)
            && collide(firstBox, CollisionShape{Circle{{7, 0}, 3}}, overlap),
        "Overlap must find the reversed circle-box pair.");
    require(overlap.normal == manifold.normal && closeTo(overlap.depth, manifold.depth, 1e-5F),
        "Reversed circle-box overlap and contact must return the same manifold.");
}

void testProperties()
{
    TestWorld world;
    const ObjectId circleId = world.addCircle({100, 200}, 10);
    world.body(circleId).body().density = 2;
    const float circleMass = 2 * Pi * 100;
    require(closeTo(bodyMass(world.body(circleId)), circleMass, 1e-3F), "Circle mass failed.");
    require(closeTo(bodyInertia(world.body(circleId)), circleMass * 50, 1e-2F), "Circle inertia failed.");

    const ObjectId boxId = world.addBox({}, {20, 40});
    require(closeTo(bodyMass(world.body(boxId)), 800, 1e-3F), "Box mass failed.");
    require(closeTo(bodyInertia(world.body(boxId)), 800 * (400 + 1600) / 12.0F, 1e-1F),
        "Box inertia failed.");

    // Смещение формы учитывается параллельным переносом.
    auto offset = std::make_unique<GameObject>();
    offset->collider() = Collider{Rect{{10, 0}, {20, 20}}};
    offset->enablePhysics(dynamicBody());
    const ObjectId offsetId = world.addObject(std::move(offset));
    const float offsetInertia = 400 * (400 + 400) / 12.0F + 400 * 500;
    require(closeTo(bodyInertia(world.body(offsetId)), offsetInertia, 1e-1F), "Offset inertia failed.");
    const CollisionShape offsetWorld = world.body(offsetId).collider().worldShape(world.body(offsetId).transform());
    require(offsetWorld.kind() == CollisionShape::Kind::Rect
            && closeTo(offsetWorld.rect().position.x, 10, 1e-5F),
        "Offset world shape failed.");

    // Масштаб объекта учитывается геометрией и массой: сторона 20 при масштабе 2.
    auto scaled = std::make_unique<GameObject>();
    scaled->collider() = Collider{Box{{0, 0}, {20, 20}, 0}};
    scaled->transform().scale = {2, 2};
    scaled->enablePhysics(dynamicBody());
    const ObjectId scaledId = world.addObject(std::move(scaled));
    require(closeTo(bodyMass(world.body(scaledId)), 1600, 1e-2F), "Scaled mass failed.");

    // Форма повёрнутого тела в мире.
    auto turned = std::make_unique<GameObject>();
    turned->collider() = Collider{Box{{0, 0}, {20, 10}, 0}};
    turned->transform().position = {100, 100};
    turned->transform().rotation = Pi / 2;
    turned->enablePhysics(dynamicBody());
    const ObjectId turnedId = world.addObject(std::move(turned));
    const CollisionShape turnedWorld = world.body(turnedId).collider().worldShape(world.body(turnedId).transform());
    require(turnedWorld.kind() == CollisionShape::Kind::Box
            && closeTo(turnedWorld.box().rotation, Pi / 2, 1e-5F),
        "Turned world shape failed.");

    // Статическое и кинематическое тело игнорируют силы и импульсы.
    const ObjectId fixedId = world.addBox({}, {10, 10}, BodyType::Static);
    applyImpulse(world.body(fixedId), {100, 0});
    applyForce(world.body(fixedId), {100, 0});
    require(world.body(fixedId).body().velocity == Vector2{} && world.body(fixedId).body().force == Vector2{},
        "Static body must ignore forces and impulses.");
    const ObjectId kinematicId = world.addBox({}, {10, 10}, BodyType::Kinematic);
    applyForce(world.body(kinematicId), {100, 0});
    applyImpulse(world.body(kinematicId), {100, 0});
    require(world.body(kinematicId).body().force == Vector2{}
            && world.body(kinematicId).body().velocity == Vector2{},
        "Kinematic body must ignore forces and impulses.");
}

void testIntegration()
{
    TestWorld world;
    world.physics().gravity = {0, 1000};
    const ObjectId id = world.addCircle({0, 0}, 5);
    world.steps(60);
    GameObject& body = world.body(id);
    require(closeTo(body.body().velocity.y, 1000, 0.5F), "Gravity integration failed.");
    require(body.body().velocity.x == 0, "Free fall produced horizontal velocity.");
    // Дискретное интегрирование: скорость на шаге k равна k * g * dt.
    const float expectedFall = 1000 * StepTime * StepTime * (60 * 61 / 2);
    require(closeTo(body.transform().position.y, expectedFall, 1), "Fall distance failed.");

    TestWorld damping;
    damping.physics().gravity = {0, 1000};
    const ObjectId slowId = damping.addCircle({0, 0}, 5);
    damping.body(slowId).body().linearDamping = 1;
    damping.step();
    require(closeTo(damping.body(slowId).body().velocity.y, 1000 * StepTime / (1 + StepTime), 1e-3F),
        "Linear damping failed.");

    TestWorld kinematic;
    kinematic.physics().gravity = {0, 1000};
    const ObjectId moverId = kinematic.addBox({0, 0}, {10, 10}, BodyType::Kinematic);
    kinematic.body(moverId).body().velocity = {50, 0};
    kinematic.steps(10);
    require(closeTo(kinematic.body(moverId).transform().position.x, 50 * StepTime * 10, 1e-3F)
            && kinematic.body(moverId).transform().position.y == 0,
        "Kinematic body must move by its velocity only.");

    TestWorld fixed;
    fixed.physics().gravity = {0, 1000};
    const ObjectId fixedId = fixed.addBox({0, 0}, {10, 10}, BodyType::Static);
    fixed.step();
    require(fixed.body(fixedId).transform().position == Vector2{}, "Static body moved.");
}

void testForces()
{
    TestWorld world;
    world.physics().gravity = {};
    const ObjectId id = world.addBox({0, 0}, {10, 10});
    applyForce(world.body(id), {1000, 0});
    world.step();
    const float expected = 1000 * (1.0F / 100) * StepTime;
    require(closeTo(world.body(id).body().velocity.x, expected, 1e-4F), "Force did not change velocity.");
    world.step();
    require(closeTo(world.body(id).body().velocity.x, expected, 1e-4F), "Force was not cleared after the step.");

    const ObjectId offsetId = world.addBox({0, -100}, {100, 20});
    applyForceAtPoint(world.body(offsetId), {0, 100}, {40, -100});
    world.step();
    require(world.body(offsetId).body().angularVelocity > 0, "Force at a point must add angular velocity.");

    const ObjectId impulseId = world.addBox({0, 100}, {10, 10});
    applyImpulse(world.body(impulseId), {200, 0});
    require(closeTo(world.body(impulseId).body().velocity.x, 2, 1e-5F), "Impulse did not change velocity.");
    applyImpulseAtPoint(world.body(impulseId), {200, 0}, {0, 100 - 5});
    require(world.body(impulseId).body().angularVelocity > 0, "Impulse at a point must add angular velocity.");

    const ObjectId torqueId = world.addCircle({0, 200}, 5);
    applyTorque(world.body(torqueId), 50);
    world.step();
    require(world.body(torqueId).body().angularVelocity > 0, "Torque did not change angular velocity.");

    // Угловой импульс, равный моменту инерции, даёт один радиан в секунду.
    const ObjectId spinId = world.addCircle({0, 300}, 5);
    applyAngularImpulse(world.body(spinId), bodyInertia(world.body(spinId)));
    require(closeTo(world.body(spinId).body().angularVelocity, 1, 1e-5F),
        "Angular impulse did not change angular velocity.");
    const ObjectId fixedId = world.addBox({0, 400}, {10, 10}, BodyType::Static);
    applyAngularImpulse(world.body(fixedId), 5);
    require(world.body(fixedId).body().angularVelocity == 0, "Angular impulse must not move a static body.");
}

void testCollisions()
{
    TestWorld world;
    world.physics().gravity = {0, 900};
    world.addFloor(0);
    const ObjectId ball = world.addCircle({0, -100}, 10);
    world.steps(240);
    require(closeTo(world.body(ball).transform().position.y, -10, 1), "Ball did not rest on the floor.");
    require(std::fabs(world.body(ball).body().velocity.y) < 1, "Resting ball keeps falling.");

    TestWorld headOn;
    headOn.physics().gravity = {};
    const ObjectId firstId = headOn.addCircle({-30, 0}, 10);
    headOn.body(firstId).body().velocity = {200, 0};
    headOn.body(firstId).collider().restitution = 1;
    const ObjectId secondId = headOn.addCircle({30, 0}, 10);
    headOn.body(secondId).body().velocity = {-200, 0};
    headOn.body(secondId).collider().restitution = 1;
    headOn.steps(120);
    const Vector2 left = headOn.body(firstId).transform().position;
    const Vector2 right = headOn.body(secondId).transform().position;
    require(left.x < right.x, "Elastic pair passed through itself.");
    require(distance(left, right) > 19.5F, "Elastic pair remained overlapped.");
    require(closeTo(headOn.body(firstId).body().velocity.x, -200, 1)
            && closeTo(headOn.body(secondId).body().velocity.x, 200, 1),
        "Elastic impact must swap velocities.");

    TestWorld boxPair;
    boxPair.physics().gravity = {0, 900};
    boxPair.addFloor(0);
    const ObjectId crate = boxPair.addBox({0, -100}, {40, 20});
    boxPair.steps(180);
    require(closeTo(boxPair.body(crate).transform().position.y, -10, 1), "Box did not rest on the floor.");
    require(std::fabs(boxPair.body(crate).transform().rotation) < 0.01F, "Resting box drifted in rotation.");
}

void testRestitution()
{
    TestWorld world;
    world.physics().gravity = {0, 1000};
    world.addFloor(0);
    const ObjectId id = world.addCircle({0, -100}, 8);
    world.body(id).collider().restitution = 1;
    bool landed = false;
    float peak = 0;
    for (int step = 0; step < 600; ++step) {
        world.step();
        const float y = world.body(id).transform().position.y;
        landed = landed || y > -9;
        if (landed) {
            peak = std::min(peak, y);
        }
    }
    require(landed, "Bouncing ball never reached the floor.");
    require(peak < -70, "Bouncing ball lost too much energy.");

    TestWorld dull;
    dull.physics().gravity = {0, 1000};
    dull.addFloor(0);
    const ObjectId quietId = dull.addCircle({0, -100}, 8);
    bool quietLanded = false;
    float rebound = 0;
    for (int step = 0; step < 600; ++step) {
        dull.step();
        const float y = dull.body(quietId).transform().position.y;
        quietLanded = quietLanded || y > -9;
        if (quietLanded) {
            rebound = std::min(rebound, y + 8);
        }
    }
    require(rebound > -3, "Inelastic ball bounced.");
    require(std::fabs(dull.body(quietId).body().velocity.y) < 1, "Inelastic ball did not settle.");

    // Упругость не накачивает энергию: скорость отскока равна скорости удара,
    // а шар с e=1 не поднимается выше начальной высоты за много отскоков.
    TestWorld elastic;
    elastic.physics().gravity = {0, 900};
    elastic.addFloor(0);
    const ObjectId bouncerId = elastic.addCircle({0, -200}, 20);
    elastic.body(bouncerId).collider().restitution = 1;
    elastic.body(bouncerId).collider().friction = 0.1F;
    bool elasticLanded = false;
    float elasticPeak = 0;
    float impact = 0;
    float reboundSpeed = 0;
    float previousSpeed = 0;
    for (int step = 0; step < 600; ++step) {
        elastic.step();
        const PhysicsBody& body = elastic.body(bouncerId).body();
        if (previousSpeed > 100 && body.velocity.y < 0 && reboundSpeed == 0) {
            impact = previousSpeed;
            reboundSpeed = -body.velocity.y;
        }
        previousSpeed = body.velocity.y;
        const float y = elastic.body(bouncerId).transform().position.y;
        elasticLanded = elasticLanded || y > -21;
        if (elasticLanded) {
            elasticPeak = std::min(elasticPeak, y);
        }
    }
    require(impact > 0 && closeTo(reboundSpeed, impact, impact * 0.01F),
        "Restitution must preserve the impact speed.");
    require(elasticPeak > -208 && elasticPeak < -196,
        "Elastic ball must neither gain nor lose height.");
}

void testFriction()
{
    TestWorld world;
    world.physics().gravity = {0, 1000};
    world.addFloor(0);
    const ObjectId id = world.addBox({0, -20}, {40, 40});
    world.body(id).collider().friction = 1;
    world.body(id).body().velocity = {300, 0};
    world.steps(120);
    require(std::fabs(world.body(id).body().velocity.x) < 5, "Friction did not stop the sliding box.");

    TestWorld slippery;
    slippery.physics().gravity = {0, 1000};
    slippery.addFloor(0);
    const ObjectId sliderId = slippery.addBox({0, -20}, {40, 40});
    slippery.body(sliderId).collider().friction = 0;
    slippery.body(sliderId).body().velocity = {300, 0};
    slippery.steps(120);
    require(closeTo(slippery.body(sliderId).body().velocity.x, 300, 1), "Frictionless box lost speed.");
}

void testStack()
{
    TestWorld world;
    world.physics().gravity = {0, 1000};
    const ObjectId floorId = world.addFloor(0, 400);
    world.body(floorId).collider().friction = 1;
    std::vector<ObjectId> ids;
    for (int level = 0; level < 4; ++level) {
        const ObjectId id = world.addBox({0, -20 - 40 * static_cast<float>(level)}, {40, 40});
        world.body(id).collider().friction = 0.8F;
        ids.push_back(id);
    }
    world.steps(600);
    for (std::size_t level = 0; level < ids.size(); ++level) {
        const GameObject& body = world.body(ids[level]);
        const Vector2 position = body.transform().position;
        require(std::isfinite(position.x) && std::isfinite(position.y), "Stack produced non-finite state.");
        require(std::fabs(position.x) < 2, "Stack slid sideways.");
        require(closeTo(position.y, -20 - 40 * static_cast<float>(level), 4),
            "Stack level " + std::to_string(level) + " sank or floated.");
        require(std::fabs(body.transform().rotation) < 0.05F,
            "Stack level " + std::to_string(level) + " tilted.");
    }
    require(world.body(ids[0]).transform().position.y > -21, "Bottom box sank into the floor.");
}

void testRotation()
{
    TestWorld world;
    world.physics().gravity = {};
    const ObjectId id = world.addBox({0, 0}, {100, 20});
    applyImpulseAtPoint(world.body(id), {0, 5000}, {40, 0});
    const float startAngle = world.body(id).transform().rotation;
    world.steps(60);
    require(std::fabs(world.body(id).transform().rotation - startAngle) > 0.05F,
        "Impulse did not rotate the body.");

    TestWorld damped;
    damped.physics().gravity = {};
    const ObjectId slowId = damped.addBox({0, 0}, {100, 20});
    damped.body(slowId).body().angularDamping = 2;
    damped.body(slowId).body().angularVelocity = 6;
    damped.steps(60);
    require(std::fabs(damped.body(slowId).body().angularVelocity) < 1, "Angular damping failed.");

    TestWorld landing;
    landing.physics().gravity = {0, 1000};
    landing.addFloor(0);
    const ObjectId landingId = landing.addBox({0, -100}, {40, 40}, BodyType::Dynamic, 0.6F);
    landing.body(landingId).body().angularVelocity = 4;
    landing.steps(600);
    require(landing.body(landingId).transform().position.y < -10
            && landing.body(landingId).transform().position.y > -60,
        "Spinning box did not land.");
    require(std::fabs(landing.body(landingId).body().angularVelocity) < 0.5F,
        "Spinning box did not settle its rotation.");
}

void testSensors()
{
    TestWorld world;
    world.physics().gravity = {0, 600};
    const ObjectId gateId = world.addBox({0, 100}, {200, 40}, BodyType::Static);
    world.body(gateId).collider().trigger = true;
    const ObjectId ballId = world.addCircle({0, 0}, 10);
    int begun = 0;
    int ended = 0;
    bool sensorData = true;
    for (int step = 0; step < 120; ++step) {
        world.step();
        for (const PhysicsContactEvent& event : world.events()) {
            if (!event.contact.sensor) {
                continue;
            }
            require(event.contact.first == std::min(gateId, ballId)
                    && event.contact.second == std::max(gateId, ballId),
                "Sensor contact must name both bodies.");
            require(event.contact.normalImpulse == 0 && event.contact.tangentImpulse == 0,
                "Sensor must not apply impulses.");
            begun += event.phase == ContactPhase::Begin ? 1 : 0;
            ended += event.phase == ContactPhase::End ? 1 : 0;
            sensorData = sensorData && event.contact.pointCount >= 1;
        }
    }
    require(begun == 1 && ended == 1, "Sensor must begin and end exactly once.");
    require(sensorData, "Sensor contact must carry points.");
    require(world.body(ballId).transform().position.y > 160, "Sensor must not stop the body.");

    TestWorld masked;
    masked.physics().gravity = {0, 600};
    const ObjectId maskedGateId = masked.addBox({0, 100}, {200, 40}, BodyType::Static);
    masked.body(maskedGateId).collider().trigger = true;
    masked.body(maskedGateId).collider().mask = 2U;
    const ObjectId passingId = masked.addCircle({0, 0}, 10);
    masked.body(passingId).collider().layer = 1U;
    bool reported = false;
    for (int step = 0; step < 120; ++step) {
        masked.step();
        reported = reported || !masked.events().empty();
    }
    require(!reported, "Mask must exclude the sensor pair.");

    // Кинематическое тело сообщает контакт со статическим сенсором без отталкивания.
    TestWorld kinematicZone;
    kinematicZone.physics().gravity = {};
    const ObjectId sensorId = kinematicZone.addStatic(Circle{{0, 0}, 60}, {0, 0});
    kinematicZone.body(sensorId).collider().trigger = true;
    const ObjectId walkerId = kinematicZone.addBox({0, 0}, {20, 20}, BodyType::Kinematic);
    bool kinematicReported = false;
    for (int step = 0; step < 3; ++step) {
        kinematicZone.step();
        for (const PhysicsContactEvent& event : kinematicZone.events()) {
            kinematicReported = kinematicReported || event.contact.sensor;
        }
    }
    require(kinematicReported, "Kinematic body must report contacts with a static sensor.");
    require(kinematicZone.body(walkerId).transform().position == Vector2{},
        "Kinematic-static contact must not move the kinematic body.");
}

void testEvents()
{
    TestWorld world;
    world.physics().gravity = {0, 900};
    const ObjectId floorId = world.addFloor(0);
    const ObjectId ballId = world.addCircle({0, -60}, 10);
    int begun = 0;
    int persisted = 0;
    int ended = 0;
    bool impulse = false;
    bool normalUp = false;
    for (int step = 0; step < 120; ++step) {
        world.step();
        for (const PhysicsContactEvent& event : world.events()) {
            require(event.contact.first == std::min(floorId, ballId)
                    && event.contact.second == std::max(floorId, ballId),
                "Contact must name both bodies in identifier order.");
            begun += event.phase == ContactPhase::Begin ? 1 : 0;
            persisted += event.phase == ContactPhase::Persist ? 1 : 0;
            ended += event.phase == ContactPhase::End ? 1 : 0;
            if (event.phase == ContactPhase::Begin) {
                impulse = event.contact.normalImpulse > 0;
                normalUp = event.contact.normal.y < -0.9F;
            }
        }
    }
    require(begun == 1, "Contact must begin exactly once.");
    require(persisted > 80, "Contact must persist while the ball rests.");
    require(ended == 0, "Resting contact ended without separation.");
    require(impulse, "Begin contact must report the impact impulse.");
    require(normalUp, "Contact normal must point from the floor to the ball.");

    world.scene().destroy(ballId);
    require(world.scene().find(ballId) == nullptr, "Destroyed body must disappear immediately.");
    world.step();
    require(countPhase(world.events(), ContactPhase::End) == 1, "Removal must end the contact once.");
    world.step();
    require(world.events().empty(), "Removal must not repeat the end.");
}

void testDeferred()
{
    TestWorld world;
    world.physics().gravity = {0, 900};
    world.addFloor(0);
    const ObjectId ballId = world.addCircle({0, -40}, 10);
    // Во время обработки событий тело удаляется и вместо него добавляется новое.
    bool handled = false;
    int endedAfterRemoval = 0;
    for (int step = 0; step < 60; ++step) {
        world.step();
        const bool wasHandled = handled;
        for (const PhysicsContactEvent& event : world.events()) {
            if (event.phase != ContactPhase::Begin || handled) {
                continue;
            }
            world.scene().destroy(ballId);
            const ObjectId replacement = world.addCircle({40, -40}, 10);
            world.body(replacement).body().velocity = {10, 0};
            require(world.scene().find(replacement) != nullptr && world.scene().find(ballId) == nullptr,
                "Deferred removal or immediate addition failed.");
            handled = true;
        }
        if (wasHandled) {
            endedAfterRemoval += countPhase(world.events(), ContactPhase::End);
        }
    }
    require(handled, "Contact handling did not run.");
    require(endedAfterRemoval == 1, "Removed body must end its contact once.");
    const std::size_t count = world.scene().count();
    world.step();
    require(world.scene().count() == count, "Removed body was not released after the step.");

    // Повторное удаление и неизвестный идентификатор безопасны.
    world.scene().destroy(ballId);
    world.scene().destroy(9999);
    world.step();
    require(world.scene().count() == count, "Repeated removal changed the body count.");
}

void testSync()
{
    // Поза физического тела — трансформация объекта: после шага объект получает
    // актуальную геометрию, с которой работают отрисовка и запросы.
    Scene scene;
    scene.physics().gravity = {};
    auto object = std::make_unique<GameObject>();
    object->collider() = Collider{Box{{0, 0}, {10, 10}, 0}};
    object->transform().position = {50, 60};
    object->transform().rotation = 0.3F;
    PhysicsBody settings;
    settings.type = BodyType::Dynamic;
    settings.velocity = {20, -10};
    settings.angularVelocity = 0.5F;
    object->enablePhysics(settings);
    const ObjectId id = scene.add(std::move(object));

    scene.update(StepTime);
    GameObject* moved = scene.find(id);
    require(moved != nullptr, "Linked object disappeared.");
    require(closeTo(moved->transform().position.x, 50 + 20 * StepTime, 1e-5F)
            && closeTo(moved->transform().position.y, 60 - 10 * StepTime, 1e-5F),
        "Physics must write the body position into the object transform.");
    require(closeTo(moved->transform().rotation, 0.3F + 0.5F * StepTime, 1e-5F),
        "Physics must write the body angle into the object transform.");
    const CollisionShape world = moved->collider().worldShape(moved->transform());
    require(world.kind() == CollisionShape::Kind::Box
            && closeTo(world.box().center.x, moved->transform().position.x, 1e-5F)
            && closeTo(world.box().center.y, moved->transform().position.y, 1e-5F),
        "Collider geometry must follow the body pose.");

    // Масштаб влияет на геометрию и массу: сторона 10 при масштабе 3.
    auto scaled = std::make_unique<GameObject>();
    scaled->collider() = Collider{Box{{0, 0}, {10, 10}, 0}};
    scaled->transform().scale = {3, 3};
    scaled->enablePhysics(dynamicBody());
    const ObjectId scaledId = scene.add(std::move(scaled));
    require(closeTo(bodyMass(*scene.find(scaledId)), 900, 1e-2F),
        "Scaled mass must follow the object scale.");

    // Объект без включённого тела физикой не двигается.
    auto loose = std::make_unique<GameObject>();
    loose->collider() = Collider{Box{{0, 0}, {10, 10}, 0}};
    loose->transform().position = {-100, -100};
    const ObjectId looseId = scene.add(std::move(loose));
    scene.update(StepTime);
    require(scene.find(looseId)->transform().position == Vector2{-100, -100},
        "Object without a body must not be stepped by physics.");

    scene.destroy(id);
    scene.update(StepTime);
    require(scene.find(id) == nullptr, "Destroyed object must not stay in the scene.");
}

void testLayers()
{
    TestWorld world;
    world.physics().gravity = {};
    const ObjectId firstId = world.addCircle({0, 0}, 10);
    world.body(firstId).collider().layer = 1U;
    world.body(firstId).collider().mask = 1U;
    const ObjectId secondId = world.addCircle({5, 0}, 10);
    world.body(secondId).collider().layer = 2U;
    world.body(secondId).collider().mask = 2U;
    world.steps(30);
    require(world.events().empty(), "Masks must exclude the pair.");
    require(closeTo(world.body(firstId).transform().position.x, 0, 1e-5F)
            && closeTo(world.body(secondId).transform().position.x, 5, 1e-5F),
        "Excluded pair must not move.");
}

void testFastMovement()
{
    TestWorld world;
    world.physics().gravity = {};
    world.addBox({0, 0}, {100, 400}, BodyType::Static);
    const ObjectId missileId = world.addCircle({-300, 0}, 10);
    world.body(missileId).body().velocity = {3000, 0};
    world.steps(120);
    const Vector2 stopped = world.body(missileId).transform().position;
    require(std::isfinite(stopped.x), "Fast movement produced a non-finite state.");
    require(stopped.x < -50, "Fast body tunneled through a thick wall.");
    require(std::fabs(world.body(missileId).body().velocity.x) < 50,
        "Fast body did not lose its speed at the wall.");

    // Восьмипиксельная стена: за шаг тело проходит сто пикселей, поэтому без проверки
    // времени удара оно пролетело бы сквозь неё.
    TestWorld thin;
    thin.physics().gravity = {};
    thin.addBox({0, 0}, {8, 400}, BodyType::Static);
    const ObjectId bulletId = thin.addCircle({-300, 0}, 10);
    thin.body(bulletId).body().velocity = {5000, 0};
    thin.steps(60);
    const Vector2 swept = thin.body(bulletId).transform().position;
    require(std::isfinite(swept.x), "Swept motion produced a non-finite state.");
    require(swept.x < -10, "Swept body tunneled through a thin wall.");
    require(swept.x > -40, "Swept body must stop at the wall.");

    // Быстрое падение на пол не оставляет проникновения в поверхность.
    TestWorld drop;
    drop.physics().gravity = {0, 1600};
    constexpr float FloorSurface = 20;
    drop.addBox({0, FloorSurface + 20}, {400, 40}, BodyType::Static);
    const ObjectId fallingId = drop.addCircle({0, -300}, 10);
    float deepest = -1000;
    for (int step = 0; step < 240; ++step) {
        drop.step();
        deepest = std::max(deepest, drop.body(fallingId).transform().position.y + 10 - FloorSurface);
    }
    require(deepest < 0.5F, "Fast impact must not push the body into the floor.");
    require(deepest > -1.0F, "Fast impact must reach the floor.");

    // Быстрый шар не проходит сквозь динамический ящик.
    TestWorld dynamic;
    dynamic.physics().gravity = {};
    const ObjectId crateId = dynamic.addBox({0, 0}, {40, 40});
    dynamic.body(crateId).collider().friction = 0;
    const ObjectId bulletBallId = dynamic.addCircle({-300, 0}, 10);
    dynamic.body(bulletBallId).body().velocity = {4000, 0};
    dynamic.body(bulletBallId).collider().friction = 0;
    dynamic.steps(60);
    require(dynamic.body(bulletBallId).transform().position.x
            < dynamic.body(crateId).transform().position.x - 20,
        "Fast body must not pass through a dynamic body.");

    // Тонкая динамическая стена тоже останавливает быстрый шар.
    TestWorld thinDynamic;
    thinDynamic.physics().gravity = {};
    const ObjectId wallId = thinDynamic.addBox({0, 0}, {8, 400});
    const ObjectId pelletId = thinDynamic.addCircle({-300, 0}, 10);
    thinDynamic.body(pelletId).body().velocity = {5000, 0};
    thinDynamic.steps(60);
    require(thinDynamic.body(pelletId).transform().position.x
            <= thinDynamic.body(wallId).transform().position.x - 4 - 10 + 1,
        "Swept body tunneled through a thin dynamic wall.");

    // Быстрое кинематическое тело толкает динамическое, а не проходит сквозь него.
    TestWorld carrier;
    carrier.physics().gravity = {};
    const ObjectId targetId = carrier.addCircle({0, 0}, 10);
    const ObjectId pusherId = carrier.addBox({-400, 0}, {40, 40}, BodyType::Kinematic);
    carrier.body(pusherId).body().velocity = {3000, 0};
    carrier.steps(30);
    require(carrier.body(targetId).transform().position.x > 20,
        "Kinematic body must push instead of passing through a dynamic body.");
}

void testTallStack()
{
    TestWorld world;
    world.physics().gravity = {0, 900};
    const ObjectId floorId = world.addFloor(0, 500);
    world.body(floorId).collider().friction = 1;
    std::vector<ObjectId> ids;
    for (int level = 0; level < 8; ++level) {
        const ObjectId id = world.addBox({0, -15 - 30 * static_cast<float>(level)}, {30, 30});
        world.body(id).collider().friction = 0.8F;
        ids.push_back(id);
    }
    world.steps(600);
    for (int level = 0; level < 8; ++level) {
        const GameObject& body = world.body(ids[static_cast<std::size_t>(level)]);
        const Vector2 position = body.transform().position;
        require(std::isfinite(position.x) && std::isfinite(position.y),
            "Tall stack produced a non-finite state.");
        require(std::fabs(position.x) < 1, "Tall stack must not drift sideways.");
        require(closeTo(position.y, -15 - 30 * static_cast<float>(level), 5),
            "Tall stack level " + std::to_string(level) + " sank or floated.");
        require(std::fabs(body.transform().rotation) < 0.05F,
            "Tall stack level " + std::to_string(level) + " tilted.");
    }
}

void testSleeping()
{
    TestWorld world;
    world.physics().gravity = {0, 900};
    const ObjectId floorId = world.addFloor(0);
    world.body(floorId).collider().friction = 1;
    std::vector<ObjectId> ids;
    for (int level = 0; level < 4; ++level) {
        const ObjectId id = world.addBox({0, -15 - 30 * static_cast<float>(level)}, {30, 30});
        world.body(id).collider().friction = 0.8F;
        ids.push_back(id);
    }
    world.steps(120);
    for (const ObjectId id : ids) {
        require(world.body(id).body().sleeping, "Settled stack must fall asleep.");
        require(world.body(id).body().velocity == Vector2{}, "Sleeping body must keep zero velocity.");
    }
    std::vector<Vector2> frozen;
    for (const ObjectId id : ids) {
        frozen.push_back(world.body(id).transform().position);
    }
    world.steps(120);
    for (std::size_t index = 0; index < ids.size(); ++index) {
        require(world.body(ids[index]).transform().position == frozen[index],
            "Sleeping body must not move.");
    }
    // Удар будит спящую стопку.
    const ObjectId ballId = world.addCircle({0, -160}, 12);
    world.body(ballId).body().velocity = {0, 600};
    world.steps(30);
    bool woken = false;
    for (const ObjectId id : ids) {
        woken = woken || !world.body(id).body().sleeping;
    }
    require(woken, "Impact must wake the sleeping stack.");

    // Тело на движущейся кинематической платформе не засыпает.
    TestWorld carrier;
    carrier.physics().gravity = {0, 900};
    const ObjectId platformId = carrier.addBox({0, 0}, {120, 20}, BodyType::Kinematic);
    carrier.body(platformId).body().velocity = {40, 0};
    const ObjectId carriedId = carrier.addBox({0, -25}, {30, 30});
    carrier.body(carriedId).collider().friction = 0.9F;
    carrier.steps(120);
    require(!carrier.body(carriedId).body().sleeping, "Body on a moving platform must stay awake.");

    // Внешняя сила не даёт телу уснуть.
    TestWorld pushed;
    pushed.physics().gravity = {0, 900};
    pushed.addFloor(0);
    const ObjectId pressedId = pushed.addBox({0, -20}, {30, 30});
    pushed.body(pressedId).collider().friction = 0.5F;
    for (int step = 0; step < 120; ++step) {
        applyForce(pushed.body(pressedId), {20000, 0});
        pushed.step();
    }
    require(!pushed.body(pressedId).body().sleeping, "Applied force must keep the body awake.");

    // Кинематическое тело, перенесённое приложением через трансформацию, тоже будит стопку.
    TestWorld moved;
    moved.physics().gravity = {0, 900};
    const ObjectId movedFloor = moved.addFloor(0);
    moved.body(movedFloor).collider().friction = 1;
    std::vector<ObjectId> sleeping;
    for (int level = 0; level < 2; ++level) {
        const ObjectId id = moved.addBox({0, -15 - 30 * static_cast<float>(level)}, {30, 30});
        moved.body(id).collider().friction = 0.8F;
        sleeping.push_back(id);
    }
    moved.steps(120);
    for (const ObjectId id : sleeping) {
        require(moved.body(id).body().sleeping, "Stack must sleep before the transform impact.");
    }
    const ObjectId pusher = moved.addBox({-300, -15}, {20, 20}, BodyType::Kinematic);
    moved.body(pusher).transform().position = {0, -15};
    moved.step();
    bool wokenByTransform = false;
    for (const ObjectId id : sleeping) {
        wokenByTransform = wokenByTransform || !moved.body(id).body().sleeping;
    }
    require(wokenByTransform, "Kinematic transform movement must wake the sleeping stack.");
}

void testFixedStepReproducibility()
{
    const auto configure = [](TestWorld& world) {
        world.physics().gravity = {0, 900};
        world.addFloor(0);
        world.addCircle({-40, -200}, 12);
        const ObjectId crateId = world.addBox({40, -150}, {40, 40}, BodyType::Dynamic, 0.3F);
        world.body(crateId).body().velocity = {30, 0};
        const ObjectId heavyId = world.addBox({0, -260}, {60, 20});
        world.body(heavyId).body().density = 3;
    };
    TestWorld dense;
    TestWorld sparse;
    configure(dense);
    configure(sparse);
    dense.steps(120);
    // То же число шагов тем же размером, но разбитое по кадрам иначе.
    for (int frame = 0; frame < 40; ++frame) {
        for (int step = 0; step < 3; ++step) {
            sparse.step();
        }
    }
    const std::vector<GameObject*> first = dense.scene().findAll();
    const std::vector<GameObject*> second = sparse.scene().findAll();
    require(first.size() == second.size(), "Different body counts after equal steps.");
    for (std::size_t index = 0; index < first.size(); ++index) {
        require(closeTo(first[index]->transform().position.x, second[index]->transform().position.x, 1e-4F)
                && closeTo(first[index]->transform().position.y, second[index]->transform().position.y, 1e-4F)
                && closeTo(first[index]->transform().rotation, second[index]->transform().rotation, 1e-4F),
            "Fixed step results depend on frame grouping.");
    }

    // Переменный шаг тоже допустим: мир не требует фиксированного dT, объекты
    // и физика шагают одинаковой длительностью, которую задаёт приложение.
    TestWorld variable;
    variable.physics().gravity = {0, 900};
    variable.addFloor(0);
    const ObjectId falling = variable.addCircle({0, -100}, 10);
    const float deltas[] = {1.0F / 60, 1.0F / 50, 1.0F / 75};
    for (int step = 0; step < 120; ++step) {
        variable.scene().update(deltas[step % 3]);
    }
    const Vector2 position = variable.body(falling).transform().position;
    require(std::isfinite(position.x) && std::isfinite(position.y)
            && std::isfinite(variable.body(falling).body().velocity.y),
        "Variable step must keep finite state.");
}

void testPose()
{
    TestWorld world;
    world.physics().gravity = {};
    const ObjectId id = world.addCircle({50, 80}, 10);
    world.body(id).body().velocity = {5, -3};
    world.body(id).body().angularVelocity = 0.5F;
    world.step();
    require(closeTo(world.body(id).transform().position.x, 50 + 5 * StepTime, 1e-4F)
            && closeTo(world.body(id).transform().position.y, 80 - 3 * StepTime, 1e-4F),
        "Physics must read the object pose and integrate it.");
    require(closeTo(world.body(id).transform().rotation, 0.5F * StepTime, 1e-4F),
        "Physics must integrate the object angle.");

    // Перенос трансформации переносит тело: мир читает позу объекта в начале шага.
    world.body(id).body().velocity = {};
    world.body(id).body().angularVelocity = 0;
    world.body(id).transform().position = {200, 120};
    world.body(id).transform().rotation = 0.75F;
    world.body(id).body().wake();
    world.step();
    require(closeTo(world.body(id).transform().position.x, 200, 1e-4F)
            && closeTo(world.body(id).transform().position.y, 120, 1e-4F)
            && closeTo(world.body(id).transform().rotation, 0.75F, 1e-4F),
        "Placed body must keep its object pose.");

    // Неподвижный объект остаётся на месте.
    const ObjectId wallId = world.addBox({300, 300}, {40, 20}, BodyType::Static, 1.0F);
    world.step();
    require(closeTo(world.body(wallId).transform().position.x, 300, 1e-4F)
            && closeTo(world.body(wallId).transform().position.y, 300, 1e-4F),
        "Static body must keep its pose.");
}
}

int main(int argumentCount, char** arguments)
{
    try {
        require(argumentCount == 2, "Expected a physics test group.");
        const std::string group = arguments[1];
        if (group == "manifold") {
            testManifold();
        } else if (group == "properties") {
            testProperties();
        } else if (group == "integration") {
            testIntegration();
        } else if (group == "forces") {
            testForces();
        } else if (group == "collisions") {
            testCollisions();
        } else if (group == "restitution") {
            testRestitution();
        } else if (group == "friction") {
            testFriction();
        } else if (group == "stack") {
            testStack();
        } else if (group == "rotation") {
            testRotation();
        } else if (group == "sensors") {
            testSensors();
        } else if (group == "events") {
            testEvents();
        } else if (group == "deferred") {
            testDeferred();
        } else if (group == "sync") {
            testSync();
        } else if (group == "layers") {
            testLayers();
        } else if (group == "fast") {
            testFastMovement();
        } else if (group == "tallstack") {
            testTallStack();
        } else if (group == "sleep") {
            testSleeping();
        } else if (group == "fixedstep") {
            testFixedStepReproducibility();
        } else if (group == "pose") {
            testPose();
        } else {
            throw std::runtime_error("Unknown physics test group.");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
