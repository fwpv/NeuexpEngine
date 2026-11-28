#include "collision/contacts.h"
#include "core/geometry.h"
#include "core/math2d.h"
#include "physics/body.h"
#include "physics/manifold.h"
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

RigidBody circleBody(Vector2 position, float radius, BodyType type = BodyType::Dynamic)
{
    return RigidBody{Circle{{0, 0}, radius}, type, position};
}

RigidBody boxBody(Vector2 position, Size2 size, BodyType type = BodyType::Dynamic, float angle = 0)
{
    return RigidBody{Box{{0, 0}, size, 0}, type, position, angle};
}

// Неподвижный пол шириной width с верхней гранью на y = top.
BodyId addFloor(PhysicsWorld& world, float top, float width = 600)
{
    return world.addBody(boxBody({0, top + 20}, {width, 40}, BodyType::Static));
}

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
}

void testProperties()
{
    RigidBody circle = circleBody({100, 200}, 10);
    circle.density = 2;
    const float circleMass = 2 * Pi * 100;
    require(closeTo(circle.mass(), circleMass, 1e-3F), "Circle mass failed.");
    require(closeTo(circle.inertia(), circleMass * 50, 1e-2F), "Circle inertia failed.");
    require(closeTo(circle.inverseMass(), 1 / circleMass, 1e-5F), "Circle inverse mass failed.");

    RigidBody box = boxBody({}, {20, 40});
    box.density = 1;
    require(closeTo(box.mass(), 800, 1e-3F), "Box mass failed.");
    require(closeTo(box.inertia(), 800 * (400 + 1600) / 12.0F, 1e-1F), "Box inertia failed.");

    // Смещение формы учитывается параллельным переносом.
    RigidBody offset{Rect{{10, 0}, {20, 20}}, BodyType::Dynamic, {}};
    offset.density = 1;
    const float offsetInertia = 400 * (400 + 400) / 12.0F + 400 * 500;
    require(closeTo(offset.inertia(), offsetInertia, 1e-1F), "Offset inertia failed.");
    const CollisionShape world = offset.worldShape();
    require(world.kind() == CollisionShape::Kind::Rect && closeTo(world.rect().position.x, 10, 1e-5F),
        "Offset world shape failed.");

    // Форма повёрнутого тела в мире.
    RigidBody turned{Box{{0, 0}, {20, 10}, 0}, BodyType::Dynamic, {100, 100}, Pi / 2};
    const CollisionShape turnedWorld = turned.worldShape();
    require(turnedWorld.kind() == CollisionShape::Kind::Box && closeTo(turnedWorld.box().rotation, Pi / 2, 1e-5F),
        "Turned world shape failed.");

    RigidBody fixedBody = boxBody({}, {10, 10}, BodyType::Static);
    fixedBody.applyImpulse({100, 0});
    fixedBody.applyForce({100, 0});
    require(fixedBody.velocity == Vector2{} && fixedBody.force == Vector2{} && fixedBody.inverseMass() == 0,
        "Static body must ignore forces and impulses.");
    RigidBody kinematicBody = boxBody({}, {10, 10}, BodyType::Kinematic);
    kinematicBody.applyForce({100, 0});
    kinematicBody.applyImpulse({100, 0});
    require(kinematicBody.force == Vector2{} && kinematicBody.velocity == Vector2{},
        "Kinematic body must ignore forces and impulses.");
}

void testIntegration()
{
    PhysicsWorld world;
    world.gravity = {0, 1000};
    const BodyId id = world.addBody(circleBody({0, 0}, 5));
    for (int step = 0; step < 60; ++step) {
        world.step(StepTime);
    }
    RigidBody* body = world.body(id);
    require(body != nullptr, "Body disappeared during free fall.");
    require(closeTo(body->velocity.y, 1000, 0.5F), "Gravity integration failed.");
    require(body->velocity.x == 0, "Free fall produced horizontal velocity.");
    // Дискретное интегрирование: скорость на шаге k равна k * g * dt.
    const float expectedFall = 1000 * StepTime * StepTime * (60 * 61 / 2);
    require(closeTo(body->position.y, expectedFall, 1), "Fall distance failed.");

    PhysicsWorld damped;
    damped.gravity = {0, 1000};
    RigidBody slow = circleBody({0, 0}, 5);
    slow.linearDamping = 1;
    const BodyId slowId = damped.addBody(slow);
    damped.step(StepTime);
    require(closeTo(damped.body(slowId)->velocity.y, 1000 * StepTime / (1 + StepTime), 1e-3F),
        "Linear damping failed.");

    PhysicsWorld kinematic;
    kinematic.gravity = {0, 1000};
    RigidBody mover = boxBody({0, 0}, {10, 10}, BodyType::Kinematic);
    mover.velocity = {50, 0};
    const BodyId moverId = kinematic.addBody(mover);
    for (int step = 0; step < 10; ++step) {
        kinematic.step(StepTime);
    }
    require(closeTo(kinematic.body(moverId)->position.x, 50 * StepTime * 10, 1e-3F)
            && kinematic.body(moverId)->position.y == 0,
        "Kinematic body must move by its velocity only.");

    PhysicsWorld fixedWorld;
    fixedWorld.gravity = {0, 1000};
    const BodyId fixedId = fixedWorld.addBody(boxBody({0, 0}, {10, 10}, BodyType::Static));
    fixedWorld.step(StepTime);
    require(fixedWorld.body(fixedId)->position == Vector2{}, "Static body moved.");
}

void testForces()
{
    PhysicsWorld world;
    world.gravity = {};
    const BodyId id = world.addBody(boxBody({0, 0}, {10, 10}));
    RigidBody& body = *world.body(id);
    body.applyForce({1000, 0});
    world.step(StepTime);
    const float expected = 1000 * (1.0F / 100) * StepTime;
    require(closeTo(body.velocity.x, expected, 1e-4F), "Force did not change velocity.");
    world.step(StepTime);
    require(closeTo(body.velocity.x, expected, 1e-4F), "Force was not cleared after the step.");

    const BodyId offsetId = world.addBody(boxBody({0, -100}, {100, 20}));
    RigidBody& offset = *world.body(offsetId);
    offset.applyForceAtPoint({0, 100}, {40, -100});
    world.step(StepTime);
    require(offset.angularVelocity > 0, "Force at a point must add angular velocity.");

    const BodyId impulseId = world.addBody(boxBody({0, 100}, {10, 10}));
    RigidBody& impulse = *world.body(impulseId);
    impulse.applyImpulse({200, 0});
    require(closeTo(impulse.velocity.x, 2, 1e-5F), "Impulse did not change velocity.");
    impulse.applyImpulseAtPoint({200, 0}, {0, 100 - 5});
    require(impulse.angularVelocity > 0, "Impulse at a point must add angular velocity.");

    const BodyId torqueId = world.addBody(circleBody({0, 200}, 5));
    RigidBody& torque = *world.body(torqueId);
    torque.applyTorque(50);
    world.step(StepTime);
    require(torque.angularVelocity > 0, "Torque did not change angular velocity.");
}

void testCollisions()
{
    PhysicsWorld world;
    world.gravity = {0, 900};
    addFloor(world, 0);
    const BodyId ball = world.addBody(circleBody({0, -100}, 10));
    for (int step = 0; step < 240; ++step) {
        world.step(StepTime);
    }
    const RigidBody* resting = world.body(ball);
    require(resting != nullptr && closeTo(resting->position.y, -10, 1), "Ball did not rest on the floor.");
    require(std::fabs(resting->velocity.y) < 1, "Resting ball keeps falling.");

    PhysicsWorld headOn;
    headOn.gravity = {};
    RigidBody first = circleBody({-30, 0}, 10);
    first.velocity = {200, 0};
    first.restitution = 1;
    RigidBody second = circleBody({30, 0}, 10);
    second.velocity = {-200, 0};
    second.restitution = 1;
    const BodyId firstId = headOn.addBody(first);
    const BodyId secondId = headOn.addBody(second);
    for (int step = 0; step < 120; ++step) {
        headOn.step(StepTime);
    }
    const RigidBody* left = headOn.body(firstId);
    const RigidBody* right = headOn.body(secondId);
    require(left->position.x < right->position.x, "Elastic pair passed through itself.");
    require(distance(left->position, right->position) > 19.5F, "Elastic pair remained overlapped.");
    require(closeTo(left->velocity.x, -200, 1) && closeTo(right->velocity.x, 200, 1),
        "Elastic impact must swap velocities.");

    PhysicsWorld boxPair;
    boxPair.gravity = {0, 900};
    addFloor(boxPair, 0);
    const BodyId crate = boxPair.addBody(boxBody({0, -100}, {40, 20}));
    for (int step = 0; step < 180; ++step) {
        boxPair.step(StepTime);
    }
    const RigidBody* crateBody = boxPair.body(crate);
    require(closeTo(crateBody->position.y, -10, 1), "Box did not rest on the floor.");
    require(std::fabs(crateBody->angle) < 0.01F, "Resting box drifted in rotation.");
}

void testRestitution()
{
    PhysicsWorld world;
    world.gravity = {0, 1000};
    addFloor(world, 0);
    RigidBody ball = circleBody({0, -100}, 8);
    ball.restitution = 1;
    const BodyId id = world.addBody(ball);
    bool landed = false;
    float peak = 0;
    for (int step = 0; step < 600; ++step) {
        world.step(StepTime);
        const RigidBody* falling = world.body(id);
        landed = landed || falling->position.y > -9;
        if (landed) {
            peak = std::min(peak, falling->position.y);
        }
    }
    require(landed, "Bouncing ball never reached the floor.");
    require(peak < -70, "Bouncing ball lost too much energy.");

    PhysicsWorld dull;
    dull.gravity = {0, 1000};
    addFloor(dull, 0);
    RigidBody quiet = circleBody({0, -100}, 8);
    const BodyId quietId = dull.addBody(quiet);
    bool quietLanded = false;
    float rebound = 0;
    for (int step = 0; step < 600; ++step) {
        dull.step(StepTime);
        const RigidBody* settled = dull.body(quietId);
        quietLanded = quietLanded || settled->position.y > -9;
        if (quietLanded) {
            rebound = std::min(rebound, settled->position.y + 8);
        }
    }
    require(rebound > -3, "Inelastic ball bounced.");
    require(std::fabs(dull.body(quietId)->velocity.y) < 1, "Inelastic ball did not settle.");
}

void testFriction()
{
    PhysicsWorld world;
    world.gravity = {0, 1000};
    addFloor(world, 0);
    RigidBody crate = boxBody({0, -20}, {40, 40});
    crate.friction = 1;
    crate.velocity = {300, 0};
    const BodyId id = world.addBody(crate);
    for (int step = 0; step < 120; ++step) {
        world.step(StepTime);
    }
    require(std::fabs(world.body(id)->velocity.x) < 5, "Friction did not stop the sliding box.");

    PhysicsWorld slippery;
    slippery.gravity = {0, 1000};
    addFloor(slippery, 0);
    RigidBody slider = boxBody({0, -20}, {40, 40});
    slider.friction = 0;
    slider.velocity = {300, 0};
    const BodyId sliderId = slippery.addBody(slider);
    for (int step = 0; step < 120; ++step) {
        slippery.step(StepTime);
    }
    require(closeTo(slippery.body(sliderId)->velocity.x, 300, 1), "Frictionless box lost speed.");
}

void testStack()
{
    PhysicsWorld world;
    world.gravity = {0, 1000};
    RigidBody floorBody = boxBody({0, 20}, {400, 40}, BodyType::Static);
    floorBody.friction = 1;
    world.addBody(floorBody);
    std::vector<BodyId> ids;
    for (int level = 0; level < 4; ++level) {
        RigidBody crate = boxBody({0, -20 - 40 * static_cast<float>(level)}, {40, 40});
        crate.friction = 0.8F;
        ids.push_back(world.addBody(crate));
    }
    for (int step = 0; step < 600; ++step) {
        world.step(StepTime);
    }
    for (std::size_t level = 0; level < ids.size(); ++level) {
        const RigidBody* body = world.body(ids[level]);
        require(body != nullptr, "Stack body disappeared.");
        require(std::isfinite(body->position.x) && std::isfinite(body->position.y), "Stack produced non-finite state.");
        require(std::fabs(body->position.x) < 2, "Stack slid sideways.");
        require(closeTo(body->position.y, -20 - 40 * static_cast<float>(level), 4),
            "Stack level " + std::to_string(level) + " sank or floated.");
        require(std::fabs(body->angle) < 0.05F, "Stack level " + std::to_string(level) + " tilted.");
    }
    require(world.body(ids[0])->position.y > -21, "Bottom box sank into the floor.");
}

void testRotation()
{
    PhysicsWorld world;
    world.gravity = {};
    RigidBody spinner = boxBody({0, 0}, {100, 20});
    const BodyId id = world.addBody(spinner);
    world.body(id)->applyImpulseAtPoint({0, 5000}, {40, 0});
    const float startAngle = world.body(id)->angle;
    for (int step = 0; step < 60; ++step) {
        world.step(StepTime);
    }
    require(std::fabs(world.body(id)->angle - startAngle) > 0.05F, "Impulse did not rotate the body.");

    PhysicsWorld damped;
    damped.gravity = {};
    RigidBody slow = boxBody({0, 0}, {100, 20});
    slow.angularDamping = 2;
    slow.angularVelocity = 6;
    const BodyId slowId = damped.addBody(slow);
    for (int step = 0; step < 60; ++step) {
        damped.step(StepTime);
    }
    require(std::fabs(damped.body(slowId)->angularVelocity) < 1, "Angular damping failed.");

    PhysicsWorld landing;
    landing.gravity = {0, 1000};
    landing.addBody(boxBody({0, 20}, {400, 40}, BodyType::Static));
    RigidBody landingBody = boxBody({0, -100}, {40, 40}, BodyType::Dynamic, 0.6F);
    landingBody.angularVelocity = 4;
    const BodyId landingId = landing.addBody(landingBody);
    for (int step = 0; step < 600; ++step) {
        landing.step(StepTime);
    }
    const RigidBody* landed = landing.body(landingId);
    require(landed->position.y < -10 && landed->position.y > -60, "Spinning box did not land.");
    require(std::fabs(landed->angularVelocity) < 0.5F, "Spinning box did not settle its rotation.");
}

void testSensors()
{
    PhysicsWorld world;
    world.gravity = {0, 600};
    RigidBody gate = boxBody({0, 100}, {200, 40}, BodyType::Static);
    gate.sensor = true;
    const BodyId gateId = world.addBody(gate);
    const BodyId ballId = world.addBody(circleBody({0, 0}, 10));
    int begun = 0;
    int ended = 0;
    bool sensorData = true;
    for (int step = 0; step < 120; ++step) {
        world.step(StepTime);
        for (const PhysicsContactEvent& event : world.contactEvents()) {
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
    require(world.body(ballId)->position.y > 160, "Sensor must not stop the body.");

    PhysicsWorld masked;
    masked.gravity = {0, 600};
    RigidBody maskedGate = boxBody({0, 100}, {200, 40}, BodyType::Static);
    maskedGate.sensor = true;
    maskedGate.mask = 2U;
    masked.addBody(maskedGate);
    RigidBody passing = circleBody({0, 0}, 10);
    passing.layer = 1U;
    masked.addBody(passing);
    bool reported = false;
    for (int step = 0; step < 120; ++step) {
        masked.step(StepTime);
        reported = reported || !masked.contactEvents().empty();
    }
    require(!reported, "Mask must exclude the sensor pair.");
}

void testEvents()
{
    PhysicsWorld world;
    world.gravity = {0, 900};
    const BodyId floorId = addFloor(world, 0);
    const BodyId ballId = world.addBody(circleBody({0, -60}, 10));
    int begun = 0;
    int persisted = 0;
    int ended = 0;
    bool impulse = false;
    bool normalUp = false;
    for (int step = 0; step < 120; ++step) {
        world.step(StepTime);
        for (const PhysicsContactEvent& event : world.contactEvents()) {
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

    world.destroyBody(ballId);
    require(world.body(ballId) == nullptr, "Destroyed body must disappear immediately.");
    world.step(StepTime);
    require(countPhase(world.contactEvents(), ContactPhase::End) == 1, "Removal must end the contact once.");
    world.step(StepTime);
    require(world.contactEvents().empty(), "Removal must not repeat the end.");
}

void testDeferred()
{
    PhysicsWorld world;
    world.gravity = {0, 900};
    addFloor(world, 0);
    const BodyId ballId = world.addBody(circleBody({0, -40}, 10));
    // Во время обработки событий тело удаляется и вместо него добавляется новое.
    bool handled = false;
    int endedAfterRemoval = 0;
    for (int step = 0; step < 60; ++step) {
        world.step(StepTime);
        const bool wasHandled = handled;
        for (const PhysicsContactEvent& event : world.contactEvents()) {
            if (event.phase != ContactPhase::Begin || handled) {
                continue;
            }
            world.destroyBody(ballId);
            const BodyId replacement = world.addBody(circleBody({40, -40}, 10));
            world.body(replacement)->velocity = {10, 0};
            require(world.body(replacement) != nullptr && world.body(ballId) == nullptr,
                "Deferred removal or immediate addition failed.");
            handled = true;
        }
        if (wasHandled) {
            endedAfterRemoval += countPhase(world.contactEvents(), ContactPhase::End);
        }
    }
    require(handled, "Contact handling did not run.");
    require(endedAfterRemoval == 1, "Removed body must end its contact once.");
    const std::size_t count = world.bodyCount();
    world.step(StepTime);
    require(world.bodyCount() == count, "Removed body was not released after the step.");

    // Повторное удаление и неизвестный идентификатор безопасны.
    world.destroyBody(ballId);
    world.destroyBody(9999);
    world.step(StepTime);
    require(world.bodyCount() == count, "Repeated removal changed the body count.");
}

void testSync()
{
    Scene scene;
    auto object = std::make_unique<GameObject>();
    object->transform().position = {0, 0};
    const ObjectId objectId = scene.add(std::move(object));
    auto loose = std::make_unique<GameObject>();
    const ObjectId looseId = scene.add(std::move(loose));

    PhysicsWorld world;
    world.gravity = {};
    RigidBody body = boxBody({50, 60}, {10, 10});
    body.angle = 0.3F;
    body.velocity = {20, -10};
    body.angularVelocity = 0.5F;
    body.object = objectId;
    const BodyId bodyId = world.addBody(body);
    const BodyId looseBody = world.addBody(boxBody({-100, -100}, {10, 10}));

    world.step(StepTime);
    world.sync(scene);
    const RigidBody* simulation = world.body(bodyId);
    GameObject* moved = scene.find(objectId);
    require(moved != nullptr, "Linked object disappeared.");
    require(closeTo(moved->transform().position.x, simulation->position.x, 1e-5F)
            && closeTo(moved->transform().position.y, simulation->position.y, 1e-5F),
        "Sync must copy the body position.");
    require(closeTo(moved->transform().rotation, simulation->angle, 1e-5F), "Sync must copy the body angle.");
    require(scene.find(looseId)->transform().position == Vector2{}, "Sync touched an unlinked object.");
    // Тело без объекта синхронизацией не затрагивается.
    require(world.body(looseBody)->position != moved->transform().position,
        "Unlinked body must not be written anywhere.");

    scene.destroy(objectId);
    scene.update(0);
    world.step(StepTime);
    world.sync(scene);
}

void testLayers()
{
    PhysicsWorld world;
    world.gravity = {};
    RigidBody first = circleBody({0, 0}, 10);
    first.layer = 1U;
    first.mask = 1U;
    RigidBody second = circleBody({5, 0}, 10);
    second.layer = 2U;
    second.mask = 2U;
    const BodyId firstId = world.addBody(first);
    const BodyId secondId = world.addBody(second);
    for (int step = 0; step < 30; ++step) {
        world.step(StepTime);
    }
    require(world.contactEvents().empty(), "Masks must exclude the pair.");
    require(closeTo(world.body(firstId)->position.x, 0, 1e-5F)
            && closeTo(world.body(secondId)->position.x, 5, 1e-5F),
        "Excluded pair must not move.");
}

void testFastMovement()
{
    PhysicsWorld world;
    world.gravity = {};
    world.addBody(boxBody({0, 0}, {100, 400}, BodyType::Static));
    RigidBody missile = circleBody({-300, 0}, 10);
    missile.velocity = {3000, 0};
    const BodyId missileId = world.addBody(missile);
    for (int step = 0; step < 120; ++step) {
        world.step(StepTime);
    }
    const RigidBody* stopped = world.body(missileId);
    require(std::isfinite(stopped->position.x), "Fast movement produced a non-finite state.");
    require(stopped->position.x < -50, "Fast body tunneled through a thick wall.");
    require(std::fabs(stopped->velocity.x) < 50, "Fast body did not lose its speed at the wall.");
}

void testFixedStepReproducibility()
{
    const auto configure = [](PhysicsWorld& world) {
        world.gravity = {0, 900};
        addFloor(world, 0);
        world.addBody(circleBody({-40, -200}, 12));
        RigidBody crate = boxBody({40, -150}, {40, 40}, BodyType::Dynamic, 0.3F);
        crate.velocity = {30, 0};
        world.addBody(crate);
        RigidBody heavy = boxBody({0, -260}, {60, 20});
        heavy.density = 3;
        world.addBody(heavy);
    };
    PhysicsWorld dense;
    PhysicsWorld sparse;
    configure(dense);
    configure(sparse);
    for (int step = 0; step < 120; ++step) {
        dense.step(StepTime);
    }
    // То же число шагов тем же размером, но разбитое по кадрам иначе.
    for (int frame = 0; frame < 40; ++frame) {
        for (int step = 0; step < 3; ++step) {
            sparse.step(StepTime);
        }
    }
    const std::vector<RigidBody*> first = dense.bodies();
    const std::vector<RigidBody*> second = sparse.bodies();
    require(first.size() == second.size(), "Different body counts after equal steps.");
    for (std::size_t index = 0; index < first.size(); ++index) {
        require(closeTo(first[index]->position.x, second[index]->position.x, 1e-4F)
                && closeTo(first[index]->position.y, second[index]->position.y, 1e-4F)
                && closeTo(first[index]->angle, second[index]->angle, 1e-4F),
            "Fixed step results depend on frame grouping.");
    }
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
        } else if (group == "fixedstep") {
            testFixedStepReproducibility();
        } else {
            throw std::runtime_error("Unknown physics test group.");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
