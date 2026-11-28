#include "world.h"
#include "collision/shape.h"
#include "core/geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace PhysicsDemo
{
namespace
{
using namespace NeuexpEngine;

const Color FrameColor{0.35F, 0.4F, 0.5F, 1};
const Color ActiveFrameColor{1, 0.55F, 0.35F, 1};
const Color TitleColor{0.75F, 0.85F, 0.95F, 1};
const Color TextColor{0.85F, 0.87F, 0.92F, 1};
const Color HintColor{0.95F, 0.8F, 0.4F, 1};
const Color StaticFill{0.16F, 0.19F, 0.24F, 1};
const Color StaticOutline{0.35F, 0.4F, 0.5F, 1};
const Color KinematicFill{0.28F, 0.52F, 0.72F, 1};
const Color KinematicOutline{0.55F, 0.8F, 0.95F, 1};
const Color SensorFill{0.55F, 0.4F, 1, 0.10F};
const Color SensorOutline{0.7F, 0.55F, 1, 0.8F};
const Color BallFill{0.35F, 0.8F, 0.95F, 1};
const Color BallOutline{0.8F, 0.95F, 1, 1};
const Color CrateFill{0.72F, 0.5F, 0.3F, 1};
const Color CrateOutline{0.95F, 0.75F, 0.45F, 1};
const Color ContactColor{1, 0.85F, 0.3F, 1};
const Color LabelColor{0.6F, 0.75F, 0.9F, 1};

constexpr float TitlePixel = 2;
constexpr float TextPixel = 1;
constexpr float TitleOffset = 10;
constexpr float StateOffset = 30;
constexpr float CountersOffset = 46;
constexpr int CrateImageSize = 32;

std::string formatFloat(float value, int precision)
{
    std::ostringstream stream;
    stream << std::fixed;
    stream.precision(precision);
    stream << value;
    return stream.str();
}

Vector2 zoneCenter(const Rect& area)
{
    return {area.center().x, area.center().y + 35};
}

// Верх неподвижного пола зоны; ниже остаётся место для рамки.
float floorTop(const Rect& area)
{
    return area.position.y + area.size.height - 55;
}

BodyId addBody(PhysicsWorld& world, const CollisionShape& shape, BodyType type, Vector2 position,
    float angle = 0)
{
    return world.addBody(RigidBody{shape, type, position, angle});
}

BodyId addZoneFloor(PhysicsWorld& world, const Rect& area, float friction = 0.6F)
{
    RigidBody floor{Box{{0, 0}, {area.size.width - 120, 20}, 0}, BodyType::Static,
        {area.center().x, floorTop(area) + 10}};
    floor.friction = friction;
    return world.addBody(floor);
}

void drawShape(Renderer2D& drawing, const CollisionShape& shape, Color fill, Color outline)
{
    DrawOptions filled;
    filled.color = fill;
    DrawOptions border;
    border.color = outline;
    border.lineWidth = 2;
    if (shape.kind() == CollisionShape::Kind::Circle) {
        drawing.drawCircle(shape.circle(), filled);
        drawing.drawCircle(shape.circle(), border, ShapeFill::Outline);
        return;
    }
    if (shape.kind() == CollisionShape::Kind::Rect) {
        drawing.drawRectangle(shape.rect(), filled);
        drawing.drawRectangle(shape.rect(), border, ShapeFill::Outline);
        return;
    }
    drawing.drawPolygon(shape.box().corners(), filled);
    drawing.drawPolygon(shape.box().corners(), border, ShapeFill::Outline);
}

Image makeCrateImage()
{
    Image image{CrateImageSize, CrateImageSize,
        std::vector<std::uint8_t>(static_cast<std::size_t>(CrateImageSize) * CrateImageSize * 4, 0)};
    for (int y = 0; y < CrateImageSize; ++y) {
        for (int x = 0; x < CrateImageSize; ++x) {
            const bool border = x == 0 || y == 0 || x == CrateImageSize - 1 || y == CrateImageSize - 1;
            const bool plank = x % 8 == 0 || y % 8 == 0;
            Color color{0.66F, 0.48F, 0.28F, 1};
            if (y < CrateImageSize / 2) {
                color = {0.72F, 0.53F, 0.31F, 1};
            }
            if (plank) {
                color = {0.44F, 0.31F, 0.17F, 1};
            }
            if (border) {
                color = {0.28F, 0.18F, 0.1F, 1};
            }
            const std::size_t offset = (static_cast<std::size_t>(y) * CrateImageSize
                + static_cast<std::size_t>(x)) * 4;
            image.pixels[offset] = static_cast<std::uint8_t>(color.red * 255);
            image.pixels[offset + 1] = static_cast<std::uint8_t>(color.green * 255);
            image.pixels[offset + 2] = static_cast<std::uint8_t>(color.blue * 255);
            image.pixels[offset + 3] = 255;
        }
    }
    return image;
}

// Свободное падение: гравитация ускоряет тело, подпись показывает высоту и скорость.
class FreeFallZone final : public Zone
{
public:
    explicit FreeFallZone(Rect area)
        : Zone(area, "FREE FALL")
    {
        world().gravity = {0, 900};
        start = {zoneCenter(area).x, area.position.y + 100};
        id = addBody(world(), Circle{{0, 0}, 14}, BodyType::Dynamic, start);
    }

    std::string state() const override
    {
        const RigidBody* body = world().body(id);
        return "FALL " + formatFloat(body->position.y - start.y, 0) + "   VY "
            + formatFloat(body->velocity.y, 0);
    }

    std::string counters() const override
    {
        return "GRAVITY 900   RESTARTS " + std::to_string(restarts);
    }

private:
    void beforeStep(float) override
    {
        RigidBody* body = world().body(id);
        if (body->position.y > area().position.y + area().size.height - 20) {
            body->position = start;
            body->velocity = {};
            ++restarts;
        }
    }

    void drawExtra(Renderer2D& drawing) const override
    {
        // Отметка исходного уровня, от которого считается падение.
        DrawOptions options;
        options.color = {0.4F, 0.45F, 0.55F, 1};
        drawing.drawLine(Vector2{area().position.x + 40, start.y},
            Vector2{area().position.x + area().size.width - 40, start.y}, options);
    }

    Vector2 start;
    BodyId id = 0;
    int restarts = 0;
};

// Упругость: тело падает на пол и отскакивает; счётчик показывает удары.
class RestitutionZone final : public Zone
{
public:
    RestitutionZone(Rect area, std::string title, float restitution)
        : Zone(area, std::move(title))
        , bounce(restitution)
    {
        world().gravity = {0, 900};
        addZoneFloor(world(), area);
        start = {zoneCenter(area).x, area.position.y + 80};
        RigidBody body{Circle{{0, 0}, 14}, BodyType::Dynamic, start};
        body.restitution = restitution;
        body.friction = 0.2F;
        id = world().addBody(body);
    }

    std::string state() const override
    {
        const RigidBody* body = world().body(id);
        if (std::fabs(body->velocity.y) < 1 && body->position.y > start.y) {
            return "E " + formatFloat(bounce, 2) + "   SETTLED";
        }
        return "E " + formatFloat(bounce, 2) + "   VY " + formatFloat(body->velocity.y, 0);
    }

    std::string counters() const override
    {
        return "IMPACTS " + std::to_string(impacts) + "   LAST IMPULSE "
            + formatFloat(lastImpulse, 0);
    }

private:
    void beforeStep(float deltaTime) override
    {
        dropClock += deltaTime;
        if (dropClock < DropInterval) {
            return;
        }
        dropClock = 0;
        RigidBody* body = world().body(id);
        body->position = start;
        body->angle = 0;
        body->velocity = {};
        body->angularVelocity = 0;
    }

    void afterStep() override
    {
        bool hit = false;
        for (const PhysicsContactEvent& event : world().contactEvents()) {
            if (event.phase == ContactPhase::Begin && !event.contact.sensor) {
                ++impacts;
                lastImpulse = event.contact.normalImpulse;
                hit = true;
            }
        }
        markHighlighted(hit);
    }

    static constexpr float DropInterval = 3.5F;
    float bounce = 0;
    Vector2 start;
    BodyId id = 0;
    float dropClock = 0;
    int impacts = 0;
    float lastImpulse = 0;
};

// Линейное затухание: скорость падения стремится к пределу g / damping.
class DampingZone final : public Zone
{
public:
    explicit DampingZone(Rect area)
        : Zone(area, "LINEAR DAMPING")
    {
        world().gravity = {0, 900};
        start = {zoneCenter(area).x, area.position.y + 100};
        RigidBody body{Circle{{0, 0}, 14}, BodyType::Dynamic, start};
        body.linearDamping = Damping;
        id = world().addBody(body);
    }

    std::string state() const override
    {
        const RigidBody* body = world().body(id);
        return "VY " + formatFloat(body->velocity.y, 0) + "   LIMIT "
            + formatFloat(900.0F / Damping, 0);
    }

    std::string counters() const override
    {
        return "DAMPING " + formatFloat(Damping, 1) + "   RESTARTS " + std::to_string(restarts);
    }

private:
    void beforeStep(float) override
    {
        RigidBody* body = world().body(id);
        if (body->position.y > area().position.y + area().size.height - 20) {
            body->position = start;
            body->velocity = {};
            ++restarts;
        }
    }

    static constexpr float Damping = 2.5F;
    Vector2 start;
    BodyId id = 0;
    int restarts = 0;
};

// Угловое затухание: тело крутится и постепенно останавливается.
class AngularDampingZone final : public Zone
{
public:
    explicit AngularDampingZone(Rect area)
        : Zone(area, "ANGULAR DAMPING")
    {
        world().gravity = {};
        RigidBody body{Box{{0, 0}, {70, 26}, 0}, BodyType::Dynamic, zoneCenter(area)};
        body.angularDamping = Damping;
        body.angularVelocity = StartSpeed;
        id = world().addBody(body);
    }

    std::string state() const override
    {
        return "W " + formatFloat(world().body(id)->angularVelocity, 2) + "   START "
            + formatFloat(StartSpeed, 2);
    }

    std::string counters() const override
    {
        return "DAMPING " + formatFloat(Damping, 1) + "   RESTARTS " + std::to_string(restarts);
    }

private:
    void beforeStep(float) override
    {
        RigidBody* body = world().body(id);
        if (body->angularVelocity < ResetSpeed) {
            body->angularVelocity = StartSpeed;
            body->angle = 0;
            ++restarts;
        }
    }

    static constexpr float StartSpeed = 8;
    static constexpr float ResetSpeed = 1;
    static constexpr float Damping = 6;
    BodyId id = 0;
    int restarts = 0;
};

// Снаряд: горизонтальная скорость не меняется, гравитация добавляет падение.
class ProjectileZone final : public Zone
{
public:
    explicit ProjectileZone(Rect area)
        : Zone(area, "PROJECTILE")
    {
        world().gravity = {0, 900};
        start = {area.position.x + 50, area.position.y + 100};
        RigidBody body{Circle{{0, 0}, 12}, BodyType::Dynamic, start};
        body.velocity = {HorizontalSpeed, 0};
        id = world().addBody(body);
    }

    std::string state() const override
    {
        const RigidBody* body = world().body(id);
        return "X " + formatFloat(body->position.x - start.x, 0) + "   VX "
            + formatFloat(body->velocity.x, 0) + "   VY " + formatFloat(body->velocity.y, 0);
    }

    std::string counters() const override
    {
        return "HORIZONTAL SPEED " + formatFloat(HorizontalSpeed, 0) + " IS CONSTANT";
    }

private:
    void beforeStep(float) override
    {
        RigidBody* body = world().body(id);
        if (body->position.y > area().position.y + area().size.height - 20
            || body->position.x > area().position.x + area().size.width - 20) {
            body->position = start;
            body->velocity = {HorizontalSpeed, 0};
        }
    }

    static constexpr float HorizontalSpeed = 160;
    Vector2 start;
    BodyId id = 0;
};

// Постоянная сила: ускорение одинаково, скорость растёт.
class ForceZone final : public Zone
{
public:
    explicit ForceZone(Rect area)
        : Zone(area, "CONSTANT FORCE")
    {
        world().gravity = {};
        start = {area.position.x + 55, zoneCenter(area).y};
        id = addBody(world(), Box{{0, 0}, {40, 24}, 0}, BodyType::Dynamic, start);
    }

    std::string state() const override
    {
        return "VX " + formatFloat(world().body(id)->velocity.x, 0) + "   VY "
            + formatFloat(world().body(id)->velocity.y, 0);
    }

    std::string counters() const override
    {
        return "FORCE " + formatFloat(Acceleration * world().body(id)->mass(), 0) + "   ACCEL "
            + formatFloat(Acceleration, 0);
    }

private:
    void beforeStep(float) override
    {
        RigidBody* body = world().body(id);
        body->applyForce(Vector2{Acceleration * body->mass(), 0});
        if (body->position.x > area().position.x + area().size.width - 40) {
            body->position = start;
            body->velocity = {};
        }
    }

    static constexpr float Acceleration = 300;
    Vector2 start;
    BodyId id = 0;
};

// Сила в точке: сила приложена не к центру, поэтому тело ещё и вращается.
class ForceAtPointZone final : public Zone
{
public:
    explicit ForceAtPointZone(Rect area)
        : Zone(area, "FORCE AT POINT")
    {
        world().gravity = {};
        start = zoneCenter(area);
        id = addBody(world(), Box{{0, 0}, {64, 20}, 0}, BodyType::Dynamic, start);
    }

    std::string state() const override
    {
        const RigidBody* body = world().body(id);
        return "W " + formatFloat(body->angularVelocity, 2) + "   VY "
            + formatFloat(body->velocity.y, 0);
    }

    std::string counters() const override
    {
        return "FORCE AT X+28";
    }

private:
    void beforeStep(float) override
    {
        RigidBody* body = world().body(id);
        body->applyForceAtPoint(Vector2{0, Force}, body->position + Vector2{28, 0});
        if (body->position.y > area().position.y + area().size.height - 40) {
            body->position = start;
            body->angle = 0;
            body->velocity = {};
            body->angularVelocity = 0;
        }
    }

    static constexpr float Force = 40000;
    Vector2 start;
    BodyId id = 0;
};

// Импульс: скорость меняется скачком, между импульсами она постоянна.
class ImpulseZone final : public Zone
{
public:
    explicit ImpulseZone(Rect area)
        : Zone(area, "IMPULSE")
    {
        world().gravity = {};
        start = {area.position.x + 55, zoneCenter(area).y};
        id = addBody(world(), Box{{0, 0}, {40, 24}, 0}, BodyType::Dynamic, start);
    }

    std::string state() const override
    {
        return "VX " + formatFloat(world().body(id)->velocity.x, 0) + "   AFTER KICK";
    }

    std::string counters() const override
    {
        return "IMPULSES " + std::to_string(impulses) + "   IMPULSE "
            + formatFloat(SpeedChange * world().body(id)->mass(), 0);
    }

private:
    void beforeStep(float deltaTime) override
    {
        RigidBody* body = world().body(id);
        kickClock += deltaTime;
        if (kickClock >= KickInterval) {
            kickClock = 0;
            body->applyImpulse(Vector2{SpeedChange * body->mass(), 0});
            ++impulses;
        }
        if (body->position.x > area().position.x + area().size.width - 40) {
            body->position = start;
            body->velocity = {};
        }
    }

    static constexpr float SpeedChange = 300;
    static constexpr float KickInterval = 1.0F;
    Vector2 start;
    BodyId id = 0;
    float kickClock = 0;
    int impulses = 0;
};

// Импульс в точке: плечо относительно центра даёт угловую скорость.
class ImpulseAtPointZone final : public Zone
{
public:
    explicit ImpulseAtPointZone(Rect area)
        : Zone(area, "IMPULSE AT POINT")
    {
        world().gravity = {};
        start = zoneCenter(area);
        id = addBody(world(), Box{{0, 0}, {60, 22}, 0}, BodyType::Dynamic, start);
        kickClock = KickInterval;
    }

    std::string state() const override
    {
        const RigidBody* body = world().body(id);
        return "W " + formatFloat(body->angularVelocity, 2) + "   VY "
            + formatFloat(body->velocity.y, 0);
    }

    std::string counters() const override
    {
        return "IMPULSES " + std::to_string(impulses) + "   OFF-CENTER";
    }

private:
    void beforeStep(float deltaTime) override
    {
        RigidBody* body = world().body(id);
        kickClock += deltaTime;
        if (kickClock >= KickInterval) {
            kickClock = 0;
            body->applyImpulseAtPoint(Vector2{0, -SpeedChange * body->mass()},
                body->position + Vector2{26, 0});
            ++impulses;
        }
        if (body->position.y < area().position.y + 40) {
            body->position = start;
            body->angle = 0;
            body->velocity = {};
            body->angularVelocity = 0;
            kickClock = KickInterval;
        }
    }

    static constexpr float SpeedChange = 120;
    static constexpr float KickInterval = 1.5F;
    Vector2 start;
    BodyId id = 0;
    float kickClock = 0;
    int impulses = 0;
};

// Момент: угловая скорость растёт от постоянного момента.
class TorqueZone final : public Zone
{
public:
    explicit TorqueZone(Rect area)
        : Zone(area, "TORQUE")
    {
        world().gravity = {};
        id = addBody(world(), Box{{0, 0}, {50, 30}, 0}, BodyType::Dynamic, zoneCenter(area));
    }

    std::string state() const override
    {
        const RigidBody* body = world().body(id);
        return "W " + formatFloat(body->angularVelocity, 2) + "   ANGLE "
            + formatFloat(radiansToDegrees(body->angle), 0);
    }

    std::string counters() const override
    {
        return "ANGULAR ACCEL " + formatFloat(Acceleration, 1);
    }

private:
    void beforeStep(float) override
    {
        RigidBody* body = world().body(id);
        body->applyTorque(Acceleration * body->inertia());
        if (body->angle > 3.0F * Pi) {
            body->angle = 0;
            body->angularVelocity = 0;
        }
    }

    static constexpr float Acceleration = 2;
    BodyId id = 0;
};

// Неподвижное и кинематическое тела: силы и импульсы их не меняют.
class ForcesIgnoredZone final : public Zone
{
public:
    explicit ForcesIgnoredZone(Rect area)
        : Zone(area, "STATIC / KINEMATIC")
    {
        world().gravity = {};
        const Vector2 center = zoneCenter(area);
        staticStart = {area.position.x + 60, center.y - 35};
        staticId = addBody(world(), Box{{0, 0}, {40, 24}, 0}, BodyType::Static, staticStart);
        kinematicId = addBody(world(), Box{{0, 0}, {40, 24}, 0}, BodyType::Kinematic,
            {area.position.x + 60, center.y + 35});
        world().body(kinematicId)->velocity = {KinematicSpeed, 0};
    }

    std::string state() const override
    {
        return "STATIC SHIFT " + formatFloat(world().body(staticId)->position.x - staticStart.x, 0)
            + "   KINEMATIC VX " + formatFloat(world().body(kinematicId)->velocity.x, 0);
    }

    std::string counters() const override
    {
        return "FORCES AND IMPULSES IGNORED";
    }

private:
    void beforeStep(float) override
    {
        RigidBody* staticBody = world().body(staticId);
        staticBody->applyForce(Vector2{Force, 0});
        staticBody->applyImpulse(Vector2{Impulse, 0});
        RigidBody* kinematic = world().body(kinematicId);
        kinematic->applyForce(Vector2{Force, 0});
        kinematic->applyImpulse(Vector2{Impulse, 0});
        if (kinematic->position.x > area().position.x + area().size.width - 40) {
            kinematic->position.x = area().position.x + 40;
        }
    }

    static constexpr float KinematicSpeed = 60;
    static constexpr float Force = 5000;
    static constexpr float Impulse = 500;
    Vector2 staticStart;
    BodyId staticId = 0;
    BodyId kinematicId = 0;
};

// Плотность задаёт массу: одинаковая сила даёт разное ускорение.
class DensityZone final : public Zone
{
public:
    explicit DensityZone(Rect area)
        : Zone(area, "DENSITY TO MASS")
    {
        world().gravity = {};
        const Vector2 center = zoneCenter(area);
        startX = area.position.x + 50;
        RigidBody light{Box{{0, 0}, {40, 20}, 0}, BodyType::Dynamic, {startX, center.y - 40}};
        light.density = 1;
        RigidBody heavy{Box{{0, 0}, {40, 20}, 0}, BodyType::Dynamic, {startX, center.y + 40}};
        heavy.density = 4;
        lightId = world().addBody(light);
        heavyId = world().addBody(heavy);
    }

    std::string state() const override
    {
        return "MASS " + formatFloat(world().body(lightId)->mass(), 0) + " / "
            + formatFloat(world().body(heavyId)->mass(), 0);
    }

    std::string counters() const override
    {
        const float lightMass = world().body(lightId)->mass();
        const float heavyMass = world().body(heavyId)->mass();
        return "SAME FORCE " + formatFloat(Force, 0) + "   ACCEL " + formatFloat(Force / lightMass, 1)
            + " / " + formatFloat(Force / heavyMass, 1);
    }

private:
    void beforeStep(float) override
    {
        RigidBody* light = world().body(lightId);
        RigidBody* heavy = world().body(heavyId);
        light->applyForce(Vector2{Force, 0});
        heavy->applyForce(Vector2{Force, 0});
        if (light->position.x > area().position.x + area().size.width - 40
            || heavy->position.x > area().position.x + area().size.width - 40) {
            light->position.x = startX;
            heavy->position.x = startX;
            light->velocity = {};
            heavy->velocity = {};
        }
    }

    static constexpr float Force = 4000;
    float startX = 0;
    BodyId lightId = 0;
    BodyId heavyId = 0;
};

// Одинаковый импульс разным массам даёт скорости обратно пропорционально массе.
class MassImpulseZone final : public Zone
{
public:
    explicit MassImpulseZone(Rect area)
        : Zone(area, "MASS AND IMPULSE")
    {
        world().gravity = {};
        const Vector2 center = zoneCenter(area);
        startX = area.position.x + 50;
        RigidBody light{Box{{0, 0}, {40, 20}, 0}, BodyType::Dynamic, {startX, center.y - 40}};
        light.density = 1;
        RigidBody heavy{Box{{0, 0}, {40, 20}, 0}, BodyType::Dynamic, {startX, center.y + 40}};
        heavy.density = 4;
        lightId = world().addBody(light);
        heavyId = world().addBody(heavy);
        kickClock = KickInterval;
    }

    std::string state() const override
    {
        return "V " + formatFloat(world().body(lightId)->velocity.x, 0) + " / "
            + formatFloat(world().body(heavyId)->velocity.x, 0);
    }

    std::string counters() const override
    {
        return "SAME IMPULSE " + formatFloat(SpeedChange * world().body(lightId)->mass(), 0)
            + "   MASS " + formatFloat(world().body(lightId)->mass(), 0) + " / "
            + formatFloat(world().body(heavyId)->mass(), 0);
    }

private:
    void beforeStep(float deltaTime) override
    {
        kickClock += deltaTime;
        if (kickClock < KickInterval) {
            return;
        }
        kickClock = 0;
        RigidBody* light = world().body(lightId);
        RigidBody* heavy = world().body(heavyId);
        light->position.x = startX;
        heavy->position.x = startX;
        light->velocity = {};
        heavy->velocity = {};
        // Один и тот же импульс: лёгкое тело получает скорость в четыре раза больше.
        light->applyImpulse(Vector2{SpeedChange * light->mass(), 0});
        heavy->applyImpulse(Vector2{SpeedChange * light->mass(), 0});
    }

    static constexpr float SpeedChange = 150;
    static constexpr float KickInterval = 1.5F;
    float startX = 0;
    BodyId lightId = 0;
    BodyId heavyId = 0;
    float kickClock = 0;
};

// Трение: тело скользит по полу и останавливается; без трения скорость сохраняется.
class FrictionSlideZone final : public Zone
{
public:
    FrictionSlideZone(Rect area, std::string title, float friction)
        : Zone(area, std::move(title))
        , surfaceFriction(friction)
    {
        world().gravity = {0, 900};
        addZoneFloor(world(), area, friction);
        start = {area.position.x + 55, floorTop(area) - 12};
        RigidBody body{Box{{0, 0}, {40, 24}, 0}, BodyType::Dynamic, start};
        body.friction = friction;
        body.velocity = {StartSpeed, 0};
        id = world().addBody(body);
    }

    std::string state() const override
    {
        return "VX " + formatFloat(world().body(id)->velocity.x, 0);
    }

    std::string counters() const override
    {
        return "FRICTION " + formatFloat(surfaceFriction, 1) + "   SLID "
            + formatFloat(world().body(id)->position.x - start.x, 0) + "   RUNS "
            + std::to_string(runs);
    }

private:
    void beforeStep(float deltaTime) override
    {
        RigidBody* body = world().body(id);
        if (std::fabs(body->velocity.x) < 1) {
            stopClock += deltaTime;
        } else {
            stopClock = 0;
        }
        const bool escaped = body->position.x > area().position.x + area().size.width - 30;
        if (stopClock > 1.5F || escaped) {
            body->position = start;
            body->velocity = {StartSpeed, 0};
            stopClock = 0;
            ++runs;
        }
    }

    static constexpr float StartSpeed = 300;
    float surfaceFriction = 0;
    Vector2 start;
    BodyId id = 0;
    float stopClock = 0;
    int runs = 0;
};

// Сила против трения: слабая сила не сдвигает тело, сильная — сдвигает.
class StickSlipZone final : public Zone
{
public:
    explicit StickSlipZone(Rect area)
        : Zone(area, "FORCE VS FRICTION")
    {
        world().gravity = {0, 900};
        addZoneFloor(world(), area, 0.8F);
        start = {area.position.x + 55, floorTop(area) - 12};
        RigidBody body{Box{{0, 0}, {40, 24}, 0}, BodyType::Dynamic, start};
        body.friction = 0.8F;
        id = world().addBody(body);
    }

    std::string state() const override
    {
        const bool strong = phaseClock >= PhaseTime;
        return (strong ? "FORCE OVER LIMIT" : "FORCE BELOW LIMIT") + std::string("   VX ")
            + formatFloat(world().body(id)->velocity.x, 0);
    }

    std::string counters() const override
    {
        return "LIMIT " + formatFloat(FrictionLimit(), 0) + "   LOW " + formatFloat(LowFactor, 1)
            + " x   HIGH " + formatFloat(HighFactor, 1) + " x";
    }

private:
    float FrictionLimit() const
    {
        return 0.8F * world().body(id)->mass() * 900;
    }

    void beforeStep(float deltaTime) override
    {
        phaseClock += deltaTime;
        if (phaseClock >= PhaseTime * 2) {
            phaseClock = 0;
            RigidBody* body = world().body(id);
            body->position = start;
            body->velocity = {};
        }
        RigidBody* body = world().body(id);
        if (body->position.x > area().position.x + area().size.width - 30) {
            body->position = start;
            body->velocity = {};
        }
        const float factor = phaseClock < PhaseTime ? LowFactor : HighFactor;
        body->applyForce(Vector2{FrictionLimit() * factor, 0});
    }

    static constexpr float LowFactor = 0.6F;
    static constexpr float HighFactor = 1.5F;
    static constexpr float PhaseTime = 2.0F;
    Vector2 start;
    BodyId id = 0;
    float phaseClock = 0;
};

// Стопка тел: ящики стоят друг на друге и не расходятся.
class StackZone final : public Zone
{
public:
    explicit StackZone(Rect area)
        : Zone(area, "STACK")
    {
        world().gravity = {0, 900};
        RigidBody floor{Box{{0, 0}, {240, 20}, 0}, BodyType::Static,
            {area.center().x, floorTop(area) + 10}};
        floor.friction = 1;
        world().addBody(floor);
        for (int level = 0; level < Levels; ++level) {
            RigidBody crate{Box{{0, 0}, {36, 36}, 0}, BodyType::Dynamic,
                {area.center().x, floorTop(area) - 18 - 36.0F * static_cast<float>(level)}};
            crate.friction = 0.7F;
            ids.push_back(world().addBody(crate));
        }
    }

    std::string state() const override
    {
        float drift = 0;
        for (const BodyId id : ids) {
            drift = std::max(drift, std::fabs(world().body(id)->position.x - area().center().x));
        }
        return "LEVELS " + std::to_string(Levels) + "   DRIFT " + formatFloat(drift, 1)
            + (drift < 1 ? "   STABLE" : "   MOVING");
    }

    std::string counters() const override
    {
        return "FRICTION 0.7   GRAVITY 900";
    }

private:
    static constexpr int Levels = 4;
    std::vector<BodyId> ids;
};

// Сенсор: тело проходит сквозь него, а зона считает начало и конец контакта.
class SensorPassZone final : public Zone
{
public:
    explicit SensorPassZone(Rect area)
        : Zone(area, "SENSOR PASS")
    {
        world().gravity = {};
        const Vector2 center = zoneCenter(area);
        RigidBody sensor{Circle{{0, 0}, 65}, BodyType::Static, center};
        sensor.sensor = true;
        sensorId = world().addBody(sensor);
        start = {area.position.x + 40, center.y};
        RigidBody ball{Circle{{0, 0}, 16}, BodyType::Dynamic, start};
        ball.velocity = {Speed, 0};
        ballId = world().addBody(ball);
    }

    std::string state() const override
    {
        return "INSIDE " + std::to_string(inside) + "   SPEED "
            + formatFloat(world().body(ballId)->velocity.x, 0);
    }

    std::string counters() const override
    {
        return "BEGIN " + std::to_string(begun) + "   END " + std::to_string(ended);
    }

private:
    void beforeStep(float) override
    {
        RigidBody* ball = world().body(ballId);
        if (ball->position.x > area().position.x + area().size.width - 40) {
            ball->position = start;
            ball->velocity = {Speed, 0};
        }
    }

    void afterStep() override
    {
        for (const PhysicsContactEvent& event : world().contactEvents()) {
            if (!event.contact.sensor) {
                continue;
            }
            if (event.phase == ContactPhase::Begin) {
                ++begun;
                ++inside;
            } else if (event.phase == ContactPhase::End) {
                ++ended;
                --inside;
            }
        }
        markHighlighted(inside > 0);
    }

    static constexpr float Speed = 150;
    Vector2 start;
    BodyId sensorId = 0;
    BodyId ballId = 0;
    int begun = 0;
    int ended = 0;
    int inside = 0;
};

// Маска сенсора: при несовпадении слоёв пара исключается из проверки.
class SensorMaskZone final : public Zone
{
public:
    explicit SensorMaskZone(Rect area)
        : Zone(area, "SENSOR MASK")
    {
        world().gravity = {};
        const Vector2 center = zoneCenter(area);
        RigidBody sensor{Circle{{0, 0}, 65}, BodyType::Static, center};
        sensor.sensor = true;
        sensor.layer = 2U;
        sensor.mask = 1U;
        sensorId = world().addBody(sensor);
        start = {area.position.x + 40, center.y};
        RigidBody ball{Circle{{0, 0}, 16}, BodyType::Dynamic, start};
        ball.velocity = {Speed, 0};
        ballId = world().addBody(ball);
    }

    std::string state() const override
    {
        return (allowed() ? "PAIR ALLOWED" : "PAIR EXCLUDED") + std::string("   INSIDE ")
            + std::to_string(inside);
    }

    std::string counters() const override
    {
        return "BEGIN " + std::to_string(begun) + "   END " + std::to_string(ended);
    }

private:
    bool allowed() const
    {
        return world().body(sensorId)->mask == 1U;
    }

    void beforeStep(float deltaTime) override
    {
        toggleClock += deltaTime;
        if (toggleClock >= ToggleInterval) {
            toggleClock = 0;
            RigidBody* sensor = world().body(sensorId);
            sensor->mask = allowed() ? 2U : 1U;
        }
        RigidBody* ball = world().body(ballId);
        if (ball->position.x > area().position.x + area().size.width - 40) {
            ball->position = start;
            ball->velocity = {Speed, 0};
        }
    }

    void afterStep() override
    {
        for (const PhysicsContactEvent& event : world().contactEvents()) {
            if (!event.contact.sensor) {
                continue;
            }
            if (event.phase == ContactPhase::Begin) {
                ++begun;
                ++inside;
            } else if (event.phase == ContactPhase::End) {
                ++ended;
                --inside;
            }
        }
        markHighlighted(inside > 0);
    }

    static constexpr float Speed = 150;
    static constexpr float ToggleInterval = 2.5F;
    Vector2 start;
    BodyId sensorId = 0;
    BodyId ballId = 0;
    float toggleClock = 0;
    int begun = 0;
    int ended = 0;
    int inside = 0;
};

// Сенсор удаляет вошедшее тело: удаление отложено и безопасно во время обработки событий.
class SensorRemoveZone final : public Zone
{
public:
    explicit SensorRemoveZone(Rect area)
        : Zone(area, "SENSOR REMOVES")
    {
        world().gravity = {};
        start = {area.position.x + 20, zoneCenter(area).y};
        RigidBody sensor{Box{{0, 0}, {120, 60}, 0}, BodyType::Static, zoneCenter(area)};
        sensor.sensor = true;
        sensorId = world().addBody(sensor);
        spawnBall();
    }

    std::string state() const override
    {
        return "REMOVED " + std::to_string(removed) + "   ALIVE "
            + std::to_string(world().bodyCount() - 1);
    }

    std::string counters() const override
    {
        return "DEFERRED REMOVAL";
    }

private:
    void spawnBall()
    {
        RigidBody ball{Circle{{0, 0}, 14}, BodyType::Dynamic, start};
        ball.velocity = {Speed, 0};
        ballId = world().addBody(ball);
    }

    void beforeStep(float deltaTime) override
    {
        if (world().bodyCount() == 1) {
            respawnClock += deltaTime;
            if (respawnClock >= RespawnTime) {
                respawnClock = 0;
                spawnBall();
            }
        }
        RigidBody* ball = world().body(ballId);
        if (ball != nullptr && ball->position.x > area().position.x + area().size.width - 30) {
            ball->position = start;
        }
    }

    void afterStep() override
    {
        bool hit = false;
        for (const PhysicsContactEvent& event : world().contactEvents()) {
            if (!event.contact.sensor || event.phase != ContactPhase::Begin) {
                continue;
            }
            const BodyId other = event.contact.first == sensorId ? event.contact.second : event.contact.first;
            if (world().body(other) == nullptr) {
                continue;
            }
            world().destroyBody(other);
            ++removed;
            hit = true;
        }
        markHighlighted(hit);
    }

    static constexpr float Speed = 70;
    static constexpr float RespawnTime = 0.6F;
    Vector2 start;
    BodyId sensorId = 0;
    BodyId ballId = 0;
    float respawnClock = 0;
    int removed = 0;
};

// Импульс удара: событие начала контакта несёт глубину и величину импульса.
class ImpactImpulseZone final : public Zone
{
public:
    explicit ImpactImpulseZone(Rect area)
        : Zone(area, "IMPACT IMPULSE")
    {
        world().gravity = {0, 900};
        addZoneFloor(world(), area);
        start = {zoneCenter(area).x, area.position.y + 80};
        RigidBody ball{Circle{{0, 0}, 14}, BodyType::Dynamic, start};
        ball.restitution = 0.85F;
        id = world().addBody(ball);
    }

    std::string state() const override
    {
        return "IMPULSE " + formatFloat(lastImpulse, 0) + "   DEPTH " + formatFloat(lastDepth, 1);
    }

    std::string counters() const override
    {
        return "IMPACTS " + std::to_string(impacts);
    }

private:
    void beforeStep(float deltaTime) override
    {
        dropClock += deltaTime;
        if (dropClock < DropInterval) {
            return;
        }
        dropClock = 0;
        RigidBody* body = world().body(id);
        body->position = start;
        body->velocity = {};
        body->angularVelocity = 0;
    }

    void afterStep() override
    {
        for (const PhysicsContactEvent& event : world().contactEvents()) {
            if (event.phase == ContactPhase::Begin && !event.contact.sensor) {
                ++impacts;
                lastImpulse = event.contact.normalImpulse;
                lastDepth = event.contact.depth;
            }
        }
    }

    static constexpr float DropInterval = 2.5F;
    Vector2 start;
    BodyId id = 0;
    float dropClock = 0;
    int impacts = 0;
    float lastImpulse = 0;
    float lastDepth = 0;
};

// Точки контакта: ящик стоит на полу, зона показывает точки и глубину контакта.
class ContactPointsZone final : public Zone
{
public:
    explicit ContactPointsZone(Rect area)
        : Zone(area, "CONTACT POINTS")
    {
        world().gravity = {0, 900};
        addZoneFloor(world(), area);
        RigidBody box{Box{{0, 0}, {60, 26}, 0}, BodyType::Dynamic,
            {zoneCenter(area).x, floorTop(area) - 13}};
        box.friction = 0.8F;
        world().addBody(box);
    }

    std::string state() const override
    {
        return "POINTS " + std::to_string(pointCount) + "   DEPTH " + formatFloat(depth, 2);
    }

    std::string counters() const override
    {
        return "BOX ON FLOOR";
    }

private:
    void afterStep() override
    {
        for (const PhysicsContactEvent& event : world().contactEvents()) {
            if (event.contact.sensor || event.phase == ContactPhase::End) {
                continue;
            }
            pointCount = event.contact.pointCount;
            depth = event.contact.depth;
            points.clear();
            for (int index = 0; index < pointCount; ++index) {
                points.push_back(event.contact.points[static_cast<std::size_t>(index)]);
            }
            break;
        }
    }

    void drawExtra(Renderer2D& drawing) const override
    {
        for (const Vector2 point : points) {
            drawing.drawCircle(Circle{point, 4}, ContactColor);
        }
    }

    int pointCount = 0;
    float depth = 0;
    std::vector<Vector2> points;
};

// Кинематическая платформа везёт динамический ящик: трение передаёт движение.
class KinematicCarrierZone final : public Zone
{
public:
    explicit KinematicCarrierZone(Rect area)
        : Zone(area, "KINEMATIC CARRIER")
    {
        world().gravity = {0, 900};
        const Vector2 center = zoneCenter(area);
        RigidBody platform{Box{{0, 0}, {150, 18}, 0}, BodyType::Kinematic, {center.x, center.y + 10}};
        platform.velocity = {Speed, 0};
        platform.friction = 0.9F;
        platformId = world().addBody(platform);
        crateStart = {center.x, center.y - 14};
        RigidBody crate{Box{{0, 0}, {30, 30}, 0}, BodyType::Dynamic, crateStart};
        crate.friction = 0.9F;
        crateId = world().addBody(crate);
    }

    std::string state() const override
    {
        return "CRATE SHIFT " + formatFloat(world().body(crateId)->position.x - crateStart.x, 0);
    }

    std::string counters() const override
    {
        return "PLATFORM VX " + formatFloat(world().body(platformId)->velocity.x, 0);
    }

private:
    void beforeStep(float) override
    {
        RigidBody* platform = world().body(platformId);
        if (platform->position.x <= area().position.x + 80) {
            platform->velocity.x = Speed;
        } else if (platform->position.x >= area().position.x + area().size.width - 80) {
            platform->velocity.x = -Speed;
        }
    }

    static constexpr float Speed = 90;
    BodyId platformId = 0;
    BodyId crateId = 0;
    Vector2 crateStart;
};
}

// Большая площадка: все возможности вместе. Вместо одной проверки здесь мир,
// в котором каждая область подписана и показывает свою идею.
class PlaygroundZone final : public Zone
{
public:
    PlaygroundZone(Rect area, const Texture& texture)
        : Zone(area, "PLAYGROUND: ALL FEATURES TOGETHER")
        , crateTexture(texture)
    {
        build();
    }

    std::string state() const override
    {
        return "BODIES " + std::to_string(world().bodyCount()) + "   CONTACTS "
            + std::to_string(stats.contacts) + "   IMPACTS " + std::to_string(stats.impacts)
            + "   LAST IMPULSE " + formatFloat(stats.lastImpactImpulse, 0);
    }

    std::string counters() const override
    {
        return "GATE INSIDE " + std::to_string(stats.gateInside) + "   BEGIN "
            + std::to_string(stats.gateBegins) + "   END " + std::to_string(stats.gateEnds)
            + "   PIT REMOVED " + std::to_string(stats.pitRemoved);
    }

    void toggleWind() { wind = !wind; }
    bool isWindActive() const { return wind; }
    void setAttract(bool active) { attracting = active; }
    void setCaret(Vector2 point) { caret = point; }
    void spawnBall();
    bool impulseAt(Vector2 point);
    const PlaygroundStatistics& statistics() const { return stats; }

private:
    void build();
    void addStatic(Vector2 center, Size2 size, float angle = 0);
    BodyId addSensor(Vector2 center, Size2 size);
    void addPlatform();
    void addCrate(Vector2 position, Size2 size, float angle, float density, float angularVelocity = 0);
    void addBall(Vector2 position, float radius);
    Sprite crateSprite(Size2 size) const;

    void beforeStep(float deltaTime) override;
    void afterStep() override;
    void drawExtra(Renderer2D& drawing) const override;
    void handleEvents();

    Texture crateTexture;
    PlaygroundStatistics stats;
    BodyId platformId = 0;
    BodyId gateId = 0;
    BodyId pitId = 0;
    Vector2 caret;
    bool wind = false;
    bool attracting = false;
    int spawnIndex = 0;
    int spawnTimer = 0;
};

namespace
{
// Площадка: таймер добавляет шары, а тела ограничены, чтобы мир не рос бесконечно.
constexpr int SpawnIntervalSteps = 90;
constexpr std::size_t MaximumBodies = 34;
constexpr float WindAcceleration = 800;
constexpr float AttractionAcceleration = 1400;
constexpr float ClickImpulseSpeed = 600;
constexpr float PlatformSpeed = 90;
constexpr float PlatformLeft = 720;
constexpr float PlatformRight = 880;
}

void PlaygroundZone::build()
{
    scene().clear();
    world().clear();
    stats = {};
    platformId = 0;
    gateId = 0;
    pitId = 0;
    wind = false;
    attracting = false;
    spawnIndex = 0;
    spawnTimer = 0;
    world().gravity = {0, 900};

    // Пол с разрывом посередине: тела проваливаются в сенсор-яму и удаляются.
    addStatic({245, 656}, {430, 60});
    addStatic({880, 656}, {380, 60});
    addStatic({34, 370}, {26, 560});
    addStatic({1066, 370}, {26, 560});
    addStatic({290, 470}, {300, 22}, -0.55F);
    addStatic({780, 360}, {240, 22});
    pitId = addSensor({572, 656}, {214, 80});
    gateId = addSensor({572, 250}, {230, 110});
    addPlatform();

    addCrate({100, 180}, {40, 40}, 0.25F, 1.2F);
    addCrate({390, 200}, {40, 40}, -0.3F, 1.2F);
    addCrate({300, 170}, {40, 40}, 0.15F, 1.2F);
    addCrate({720, 220}, {60, 40}, 0.1F, 3);
    addCrate({200, 200}, {40, 40}, 0.5F, 1.2F, 1.2F);
    addCrate({800, 472}, {36, 36}, 0, 1.2F);
    addCrate({1010, 608}, {36, 36}, 0, 1.2F);
    addCrate({1010, 572}, {36, 36}, 0, 1.2F);
    addCrate({1010, 536}, {36, 36}, 0, 1.2F);

    addBall({490, 150}, 14);
    addBall({540, 160}, 16);
    addBall({600, 150}, 15);
    addBall({640, 170}, 18);
    addBall({280, 180}, 14);
    addBall({760, 160}, 16);
}

void PlaygroundZone::addStatic(Vector2 center, Size2 size, float angle)
{
    world().addBody(RigidBody{Box{{0, 0}, size, 0}, BodyType::Static, center, angle});
}

BodyId PlaygroundZone::addSensor(Vector2 center, Size2 size)
{
    RigidBody body{Box{{0, 0}, size, 0}, BodyType::Static, center};
    body.sensor = true;
    return world().addBody(body);
}

void PlaygroundZone::addPlatform()
{
    RigidBody platform{Box{{0, 0}, {150, 20}, 0}, BodyType::Kinematic, {800, 500}};
    platform.velocity = {PlatformSpeed, 0};
    platform.friction = 0.9F;
    platformId = world().addBody(platform);
}

void PlaygroundZone::addCrate(Vector2 position, Size2 size, float angle, float density,
    float angularVelocity)
{
    RigidBody body{Box{{0, 0}, size, 0}, BodyType::Dynamic, position, angle};
    body.density = density;
    body.friction = 0.55F;
    body.restitution = 0.1F;
    body.angularVelocity = angularVelocity;
    auto object = std::make_unique<GameObject>();
    object->transform().position = position;
    object->transform().rotation = angle;
    object->visual() = crateSprite(size);
    body.object = scene().add(std::move(object));
    world().addBody(body);
}

void PlaygroundZone::addBall(Vector2 position, float radius)
{
    RigidBody body{Circle{{0, 0}, radius}, BodyType::Dynamic, position};
    body.density = 0.9F;
    body.friction = 0.35F;
    body.restitution = 0.72F;
    world().addBody(body);
}

Sprite PlaygroundZone::crateSprite(Size2 size) const
{
    Sprite sprite{crateTexture};
    sprite.size = size;
    sprite.filter = TextureFilter::Nearest;
    sprite.transform.origin = Vector2{size.width / 2, size.height / 2};
    return sprite;
}

void PlaygroundZone::spawnBall()
{
    const float position = spawnIndex % 3 == 0
        ? 120.0F + static_cast<float>((spawnIndex * 37) % 220)
        : 470.0F + static_cast<float>((spawnIndex * 61) % 200);
    const float radius = 13.0F + static_cast<float>(spawnIndex % 4) * 1.5F;
    addBall({position, 150}, radius);
    ++spawnIndex;
}

bool PlaygroundZone::impulseAt(Vector2 point)
{
    const std::vector<RigidBody*> bodies = world().bodies();
    for (auto body = bodies.rbegin(); body != bodies.rend(); ++body) {
        RigidBody& candidate = **body;
        if (candidate.type != BodyType::Dynamic || !contains(candidate.worldShape(), point)) {
            continue;
        }
        candidate.applyImpulseAtPoint(Vector2{0, -ClickImpulseSpeed * candidate.mass()}, point);
        return true;
    }
    return false;
}

void PlaygroundZone::beforeStep(float)
{
    RigidBody* platform = world().body(platformId);
    if (platform != nullptr) {
        if (platform->position.x <= PlatformLeft) {
            platform->velocity.x = PlatformSpeed;
        } else if (platform->position.x >= PlatformRight) {
            platform->velocity.x = -PlatformSpeed;
        }
    }
    if (wind || attracting) {
        for (RigidBody* body : world().bodies()) {
            if (body->type != BodyType::Dynamic) {
                continue;
            }
            if (wind) {
                body->applyForce(Vector2{body->mass() * WindAcceleration, 0});
            }
            if (attracting) {
                const Vector2 offset = caret - body->position;
                if (lengthSquared(offset) > 1) {
                    body->applyForce(normalized(offset) * (body->mass() * AttractionAcceleration));
                }
            }
        }
    }
    ++spawnTimer;
    if (spawnTimer >= SpawnIntervalSteps) {
        spawnTimer = 0;
        if (world().bodyCount() < MaximumBodies) {
            spawnBall();
        }
    }
}

void PlaygroundZone::afterStep()
{
    handleEvents();
    markHighlighted(stats.gateInside > 0);
}

void PlaygroundZone::handleEvents()
{
    stats.contacts = 0;
    for (const PhysicsContactEvent& event : world().contactEvents()) {
        const PhysicsContact& contact = event.contact;
        const bool gate = contact.first == gateId || contact.second == gateId;
        const bool pit = contact.first == pitId || contact.second == pitId;
        if (contact.sensor) {
            if (gate) {
                if (event.phase == ContactPhase::Begin) {
                    ++stats.gateBegins;
                    ++stats.gateInside;
                } else if (event.phase == ContactPhase::End) {
                    ++stats.gateEnds;
                    --stats.gateInside;
                }
            }
            // Сенсор-яма удаляет вошедшее тело: удаление отложено и безопасно
            // во время обработки событий шага.
            if (pit && event.phase == ContactPhase::Begin) {
                const BodyId other = contact.first == pitId ? contact.second : contact.first;
                const RigidBody* target = world().body(other);
                if (target != nullptr) {
                    if (target->object != 0) {
                        scene().destroy(target->object);
                    }
                    world().destroyBody(other);
                    ++stats.pitRemoved;
                }
            }
            continue;
        }
        if (event.phase != ContactPhase::End) {
            ++stats.contacts;
        }
        if (event.phase == ContactPhase::Begin) {
            ++stats.impacts;
            stats.lastImpactImpulse = contact.normalImpulse;
        }
    }
}

void PlaygroundZone::drawExtra(Renderer2D& drawing) const
{
    // Подписи поясняют, какая идея проверяется в каждой области площадки.
    const struct
    {
        Vector2 at;
        const char* text;
    } labels[] = {
        {{40, 330}, "RAMP: FRICTION"},
        {{470, 500}, "BALLS: FALL + BOUNCE"},
        {{930, 470}, "STACK: STABLE"},
        {{655, 440}, "KINEMATIC CARRIER"},
        {{462, 200}, "GATE: BEGIN / END"},
        {{700, 560}, "PIT: DEFERRED REMOVAL"},
        {{560, 330}, "PLATFORM: MASS"},
    };
    for (const auto& label : labels) {
        drawing.drawDebugText(label.text, label.at, LabelColor, TextPixel);
    }
}

namespace
{
std::vector<std::unique_ptr<Zone>> buildFallingPage()
{
    std::vector<std::unique_ptr<Zone>> zones;
    zones.push_back(std::make_unique<FreeFallZone>(cellArea(0, 0)));
    zones.push_back(std::make_unique<RestitutionZone>(cellArea(1, 0), "RESTITUTION 0.9", 0.9F));
    zones.push_back(std::make_unique<RestitutionZone>(cellArea(2, 0), "RESTITUTION 0.1", 0.1F));
    zones.push_back(std::make_unique<DampingZone>(cellArea(0, 1)));
    zones.push_back(std::make_unique<AngularDampingZone>(cellArea(1, 1)));
    zones.push_back(std::make_unique<ProjectileZone>(cellArea(2, 1)));
    return zones;
}

std::vector<std::unique_ptr<Zone>> buildForcesPage()
{
    std::vector<std::unique_ptr<Zone>> zones;
    zones.push_back(std::make_unique<ForceZone>(cellArea(0, 0)));
    zones.push_back(std::make_unique<ForceAtPointZone>(cellArea(1, 0)));
    zones.push_back(std::make_unique<ImpulseZone>(cellArea(2, 0)));
    zones.push_back(std::make_unique<ImpulseAtPointZone>(cellArea(0, 1)));
    zones.push_back(std::make_unique<TorqueZone>(cellArea(1, 1)));
    zones.push_back(std::make_unique<ForcesIgnoredZone>(cellArea(2, 1)));
    return zones;
}

std::vector<std::unique_ptr<Zone>> buildMassPage()
{
    std::vector<std::unique_ptr<Zone>> zones;
    zones.push_back(std::make_unique<DensityZone>(cellArea(0, 0)));
    zones.push_back(std::make_unique<MassImpulseZone>(cellArea(1, 0)));
    zones.push_back(std::make_unique<FrictionSlideZone>(cellArea(2, 0), "FRICTION", 0.9F));
    zones.push_back(std::make_unique<FrictionSlideZone>(cellArea(0, 1), "FRICTIONLESS", 0.0F));
    zones.push_back(std::make_unique<StickSlipZone>(cellArea(1, 1)));
    zones.push_back(std::make_unique<StackZone>(cellArea(2, 1)));
    return zones;
}

std::vector<std::unique_ptr<Zone>> buildSensorsPage()
{
    std::vector<std::unique_ptr<Zone>> zones;
    zones.push_back(std::make_unique<SensorPassZone>(cellArea(0, 0)));
    zones.push_back(std::make_unique<SensorMaskZone>(cellArea(1, 0)));
    zones.push_back(std::make_unique<SensorRemoveZone>(cellArea(2, 0)));
    zones.push_back(std::make_unique<ImpactImpulseZone>(cellArea(0, 1)));
    zones.push_back(std::make_unique<ContactPointsZone>(cellArea(1, 1)));
    zones.push_back(std::make_unique<KinematicCarrierZone>(cellArea(2, 1)));
    return zones;
}
}

Rect cellArea(int column, int row)
{
    return {GridLeft + static_cast<float>(column) * (CellWidth + GridGapX),
        GridTop + static_cast<float>(row) * (CellHeight + GridGapY), CellWidth, CellHeight};
}

Zone::Zone(Rect area, std::string title)
    : zoneArea(area)
    , zoneTitle(std::move(title))
{
}

void Zone::update(float deltaTime)
{
    elapsed += deltaTime;
    beforeStep(deltaTime);
    simulation.step(deltaTime);
    simulation.sync(ownedScene);
    ownedScene.update(0);
    afterStep();
}

void Zone::beforeStep(float)
{
}

void Zone::afterStep()
{
}

void Zone::drawExtra(Renderer2D&) const
{
}

Color Zone::fillOf(const RigidBody& body) const
{
    if (body.sensor) {
        return SensorFill;
    }
    if (body.type == BodyType::Static) {
        return StaticFill;
    }
    if (body.type == BodyType::Kinematic) {
        return KinematicFill;
    }
    return body.shape.kind() == CollisionShape::Kind::Circle ? BallFill : CrateFill;
}

Color Zone::outlineOf(const RigidBody& body) const
{
    if (body.sensor) {
        return SensorOutline;
    }
    if (body.type == BodyType::Static) {
        return StaticOutline;
    }
    if (body.type == BodyType::Kinematic) {
        return KinematicOutline;
    }
    return body.shape.kind() == CollisionShape::Kind::Circle ? BallOutline : CrateOutline;
}

void Zone::draw(Renderer2D& drawing) const
{
    DrawOptions frame;
    frame.color = highlight ? ActiveFrameColor : FrameColor;
    frame.lineWidth = highlight ? 3 : 1;
    drawing.drawRectangle(zoneArea, frame, ShapeFill::Outline);

    const Vector2 origin = zoneArea.position;
    drawing.drawDebugText(zoneTitle, Vector2{origin.x + 12, origin.y + TitleOffset}, TitleColor,
        TitlePixel);
    for (const RigidBody* body : simulation.bodies()) {
        if (body->object != 0) {
            continue;
        }
        drawShape(drawing, body->worldShape(), fillOf(*body), outlineOf(*body));
    }
    ownedScene.draw(drawing);
    drawExtra(drawing);
    drawing.drawDebugText(state(), Vector2{origin.x + 12, origin.y + StateOffset}, TextColor, TextPixel);
    const std::string counterText = counters();
    if (!counterText.empty()) {
        drawing.drawDebugText(counterText, Vector2{origin.x + 12, origin.y + CountersOffset},
            HintColor, TextPixel);
    }
}

bool CheckBoard::loadAssets(Renderer2D& renderer, ResourceError& error)
{
    const Image image = makeCrateImage();
    return renderer.createTexture(image, crateTexture, error);
}

void CheckBoard::build()
{
    pages.clear();
    pages.push_back(buildFallingPage());
    pages.push_back(buildForcesPage());
    pages.push_back(buildMassPage());
    pages.push_back(buildSensorsPage());
    std::vector<std::unique_ptr<Zone>> playgroundPage;
    playgroundPage.push_back(std::make_unique<PlaygroundZone>(
        Rect{GridLeft, GridTop, WorldWidth - 2 * GridLeft, WorldHeight - GridTop - 24}, crateTexture));
    pages.push_back(std::move(playgroundPage));
    pageNames = {"FALLING AND BOUNCING", "FORCES AND IMPULSES", "MASS AND FRICTION",
        "SENSORS AND CONTACTS", "PLAYGROUND"};
    currentPage = std::min(currentPage, pageCount() - 1);
    playground = static_cast<PlaygroundZone*>(pages.back().front().get());
}

void CheckBoard::update(float deltaTime)
{
    for (const std::unique_ptr<Zone>& zone : pages[static_cast<std::size_t>(currentPage)]) {
        zone->update(deltaTime);
    }
}

void CheckBoard::draw(Renderer2D& drawing) const
{
    for (const std::unique_ptr<Zone>& zone : pages[static_cast<std::size_t>(currentPage)]) {
        zone->draw(drawing);
    }
}

void CheckBoard::nextPage()
{
    currentPage = (currentPage + 1) % pageCount();
}

int CheckBoard::pageCount() const
{
    return static_cast<int>(pages.size());
}

const std::string& CheckBoard::pageName() const
{
    return pageNames[static_cast<std::size_t>(currentPage)];
}

bool CheckBoard::isPlaygroundPage() const
{
    return currentPage == pageCount() - 1;
}

const std::vector<std::unique_ptr<Zone>>& CheckBoard::zones() const
{
    return pages[static_cast<std::size_t>(currentPage)];
}

void CheckBoard::toggleWind()
{
    if (playground != nullptr) {
        playground->toggleWind();
    }
}

bool CheckBoard::isWindActive() const
{
    return playground != nullptr && playground->isWindActive();
}

void CheckBoard::setAttract(bool active)
{
    if (playground != nullptr) {
        playground->setAttract(active);
    }
}

void CheckBoard::setCaret(Vector2 point)
{
    if (playground != nullptr) {
        playground->setCaret(point);
    }
}

bool CheckBoard::impulseAt(Vector2 point)
{
    return playground != nullptr && playground->impulseAt(point);
}

void CheckBoard::spawnBall()
{
    if (playground != nullptr) {
        playground->spawnBall();
    }
}

const PlaygroundStatistics* CheckBoard::playgroundStatistics() const
{
    return playground != nullptr ? &playground->statistics() : nullptr;
}
}
