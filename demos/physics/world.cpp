#include "world.h"
#include "collision/shape.h"
#include "core/geometry.h"

#include <algorithm>
#include <cassert>
#include <cmath>
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

constexpr float TitlePixel = 2;
constexpr float TextPixel = 1;
constexpr float TitleOffset = 10;
constexpr float StateOffset = 30;
constexpr float CountersOffset = 46;

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

// Верх неподвижного пола зоны; нижний отступ оставляет место для рамки.
float floorTop(const Rect& area, float bottomMargin = 55)
{
    return area.position.y + area.size.height - bottomMargin;
}

// Пиксельная область зоны: края расширяются наружу, чтобы содержимое у границы не пропадало.
Viewport screenViewport(const Rect& worldArea, const CameraView& view)
{
    const Vector2 first = view.worldToScreen({worldArea.position.x, worldArea.position.y});
    const Vector2 second = view.worldToScreen(
        {worldArea.position.x + worldArea.size.width, worldArea.position.y + worldArea.size.height});
    const float left = std::floor(std::min(first.x, second.x));
    const float top = std::floor(std::min(first.y, second.y));
    const float right = std::ceil(std::max(first.x, second.x));
    const float bottom = std::ceil(std::max(first.y, second.y));
    return Viewport{static_cast<int>(left), static_cast<int>(top),
        static_cast<int>(right - left), static_cast<int>(bottom - top)};
}

BodyId addBody(PhysicsWorld& world, const CollisionShape& shape, BodyType type, Vector2 position,
    float angle = 0)
{
    return world.addBody(RigidBody{shape, type, position, angle});
}

BodyId addZoneFloor(PhysicsWorld& world, const Rect& area, float friction = 0.6F,
    float bottomMargin = 55)
{
    RigidBody floor{Box{{0, 0}, {area.size.width - 120, 20}, 0}, BodyType::Static,
        {area.center().x, floorTop(area, bottomMargin) + 10}};
    floor.friction = friction;
    return world.addBody(floor);
}

// Перенос тела приложением: положение меняется, а скорость сбрасывается.
void placeBody(RigidBody& body, Vector2 position, float angle = 0)
{
    body.setPose(position, angle);
    body.velocity = {};
    body.angularVelocity = 0;
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

// Столкновения и взаимодействия: упругий отскок от пола, счётчик ударов.
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
        if (body->sleeping) {
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
        placeBody(*world().body(id), start);
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

// Трение: ящик скользит по полу и останавливается (или не останавливается без трения).
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
            placeBody(*body, start);
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

// Стопка: несколько ящиков стоят друг на друге; подпись показывает дрейф и просадку.
class StackZone final : public Zone
{
public:
    StackZone(Rect area, std::string title, int levels, float size, float bottomMargin = 55)
        : Zone(area, std::move(title))
        , count(levels)
        , boxSize(size)
    {
        world().gravity = {0, 900};
        RigidBody floor{Box{{0, 0}, {240, 20}, 0}, BodyType::Static,
            {area.center().x, floorTop(area, bottomMargin) + 10}};
        floor.friction = 1;
        world().addBody(floor);
        const float firstY = floorTop(area, bottomMargin) - boxSize / 2;
        for (int level = 0; level < count; ++level) {
            RigidBody crate{Box{{0, 0}, {boxSize, boxSize}, 0}, BodyType::Dynamic,
                {area.center().x, firstY - boxSize * static_cast<float>(level)}};
            crate.friction = 0.8F;
            ids.push_back(world().addBody(crate));
            expected.push_back(crate.position.y);
        }
    }

    std::string state() const override
    {
        float drift = 0;
        float sink = 0;
        for (std::size_t index = 0; index < ids.size(); ++index) {
            const RigidBody* body = world().body(ids[index]);
            drift = std::max(drift, std::fabs(body->position.x - area().center().x));
            sink = std::max(sink, body->position.y - expected[index]);
        }
        const bool stable = drift < 1 && sink < 3;
        return "LEVELS " + std::to_string(count) + "   DRIFT " + formatFloat(drift, 1)
            + "   SINK " + formatFloat(sink, 1) + (stable ? "   STABLE" : "   MOVING");
    }

    std::string counters() const override
    {
        return "FRICTION 0.8   GRAVITY 900";
    }

private:
    int count = 0;
    float boxSize = 0;
    std::vector<BodyId> ids;
    std::vector<float> expected;
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
        return "CRATE SHIFT " + formatFloat(std::fabs(world().body(crateId)->position.x - crateStart.x), 0);
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
            placeBody(*body, start);
        }
    }

    static constexpr float Force = 40000;
    Vector2 start;
    BodyId id = 0;
};

// События: сенсор-область сообщает о начале и конце контакта, скорость не меняется.
class SensorPassZone final : public Zone
{
public:
    explicit SensorPassZone(Rect area)
        : Zone(area, "SENSOR AREA")
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
            placeBody(*ball, start);
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
            placeBody(*ball, start);
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
            placeBody(*ball, start);
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

// Данные удара: событие начала контакта несёт глубину и величину импульса.
class ImpactImpulseZone final : public Zone
{
public:
    explicit ImpactImpulseZone(Rect area)
        : Zone(area, "IMPACT DATA")
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
        placeBody(*world().body(id), start);
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

// Фазы контакта: тело проходит сквозь область туда и обратно, видны Begin, Persist, End.
class ContactPhasesZone final : public Zone
{
public:
    explicit ContactPhasesZone(Rect area)
        : Zone(area, "CONTACT PHASES")
    {
        world().gravity = {};
        const Vector2 center = zoneCenter(area);
        RigidBody sensor{Circle{{0, 0}, 55}, BodyType::Static, center};
        sensor.sensor = true;
        world().addBody(sensor);
        start = {area.position.x + 50, center.y};
        RigidBody ball{Circle{{0, 0}, 14}, BodyType::Dynamic, start};
        ball.velocity = {Speed, 0};
        ballId = world().addBody(ball);
    }

    std::string state() const override
    {
        return "INSIDE " + std::to_string(inside) + "   LAST " + lastPhase;
    }

    std::string counters() const override
    {
        return "BEGIN " + std::to_string(begun) + "   PERSIST " + std::to_string(persisted)
            + "   END " + std::to_string(ended);
    }

private:
    void beforeStep(float) override
    {
        RigidBody* ball = world().body(ballId);
        if (ball->position.x > area().position.x + area().size.width - 40) {
            ball->velocity.x = -Speed;
        } else if (ball->position.x < area().position.x + 40) {
            ball->velocity.x = Speed;
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
                lastPhase = "BEGIN";
            } else if (event.phase == ContactPhase::Persist) {
                ++persisted;
                lastPhase = "PERSIST";
            } else {
                ++ended;
                --inside;
                lastPhase = "END";
            }
        }
        markHighlighted(inside > 0);
    }

    static constexpr float Speed = 120;
    Vector2 start;
    BodyId ballId = 0;
    int begun = 0;
    int persisted = 0;
    int ended = 0;
    int inside = 0;
    std::string lastPhase = "-";
};

// Стандарт: непрерывная проверка не даёт быстрому телу пройти сквозь тонкую статику.
class CcdStaticZone final : public Zone
{
public:
    explicit CcdStaticZone(Rect area)
        : Zone(area, "CCD VS STATIC")
    {
        world().gravity = {};
        center = zoneCenter(area);
        RigidBody wall{Box{{0, 0}, {8, 240}, 0}, BodyType::Static, center};
        wallId = world().addBody(wall);
        start = {area.position.x + 40, center.y};
        RigidBody ball{Circle{{0, 0}, 10}, BodyType::Dynamic, start};
        ball.velocity = {Speed, 0};
        ballId = world().addBody(ball);
    }

    std::string state() const override
    {
        const RigidBody* ball = world().body(ballId);
        const float gap = (center.x - 4) - (ball->position.x + 10);
        return "VX " + formatFloat(ball->velocity.x, 0) + "   GAP " + formatFloat(gap, 1);
    }

    std::string counters() const override
    {
        return "SPEED 5000   WALL 8PX   IMPACTS " + std::to_string(impacts);
    }

private:
    void beforeStep(float deltaTime) override
    {
        resetClock += deltaTime;
        if (resetClock < ResetInterval) {
            return;
        }
        resetClock = 0;
        placeBody(*world().body(ballId), start);
        world().body(ballId)->velocity = {Speed, 0};
    }

    void afterStep() override
    {
        for (const PhysicsContactEvent& event : world().contactEvents()) {
            if (event.phase == ContactPhase::Begin && !event.contact.sensor
                && (event.contact.first == wallId || event.contact.second == wallId)) {
                ++impacts;
            }
        }
    }

    static constexpr float Speed = 5000;
    static constexpr float ResetInterval = 1.6F;
    Vector2 center;
    Vector2 start;
    BodyId wallId = 0;
    BodyId ballId = 0;
    float resetClock = 0;
    int impacts = 0;
};

// Стандарт: непрерывная проверка работает и против динамической тонкой стены.
class CcdDynamicZone final : public Zone
{
public:
    explicit CcdDynamicZone(Rect area)
        : Zone(area, "CCD VS DYNAMIC")
    {
        world().gravity = {};
        center = zoneCenter(area);
        RigidBody wall{Box{{0, 0}, {8, 240}, 0}, BodyType::Dynamic, center};
        wall.friction = 0.2F;
        wall.density = 40;
        wallId = world().addBody(wall);
        start = {area.position.x + 40, center.y};
        RigidBody ball{Circle{{0, 0}, 10}, BodyType::Dynamic, start};
        ball.velocity = {Speed, 0};
        ballId = world().addBody(ball);
    }

    std::string state() const override
    {
        const RigidBody* ball = world().body(ballId);
        const RigidBody* wall = world().body(wallId);
        const float gap = (wall->position.x - 4) - (ball->position.x + 10);
        return "VX " + formatFloat(ball->velocity.x, 0) + "   GAP " + formatFloat(gap, 1);
    }

    std::string counters() const override
    {
        return "SPEED 5000   WALL DYNAMIC   IMPACTS " + std::to_string(impacts);
    }

private:
    void afterStep() override
    {
        for (const PhysicsContactEvent& event : world().contactEvents()) {
            if (event.phase == ContactPhase::Begin && !event.contact.sensor
                && (event.contact.first == wallId || event.contact.second == wallId)) {
                ++impacts;
            }
        }
    }

    void beforeStep(float deltaTime) override
    {
        resetClock += deltaTime;
        if (resetClock < ResetInterval) {
            return;
        }
        resetClock = 0;
        placeBody(*world().body(wallId), center);
        placeBody(*world().body(ballId), start);
        world().body(ballId)->velocity = {Speed, 0};
    }

    static constexpr float Speed = 5000;
    static constexpr float ResetInterval = 1.6F;
    Vector2 center;
    Vector2 start;
    BodyId wallId = 0;
    BodyId ballId = 0;
    float resetClock = 0;
    int impacts = 0;
};

// Стандарт: быстрое кинематическое тело толкает динамическое, а не проходит сквозь.
class CcdKinematicZone final : public Zone
{
public:
    explicit CcdKinematicZone(Rect area)
        : Zone(area, "CCD VS KINEMATIC")
    {
        world().gravity = {};
        pusherStart = {area.position.x + 40, zoneCenter(area).y};
        RigidBody pusher{Box{{0, 0}, {40, 40}, 0}, BodyType::Kinematic, pusherStart};
        pusher.velocity = {Speed, 0};
        pusherId = world().addBody(pusher);
        targetStart = {area.position.x + 170, zoneCenter(area).y};
        targetId = addBody(world(), Circle{{0, 0}, 12}, BodyType::Dynamic, targetStart);
    }

    std::string state() const override
    {
        return "PUSHER X " + formatFloat(world().body(pusherId)->position.x - pusherStart.x, 0)
            + "   TARGET VX " + formatFloat(world().body(targetId)->velocity.x, 0);
    }

    std::string counters() const override
    {
        return "PUSHER 3000   KINEMATIC";
    }

private:
    void beforeStep(float) override
    {
        RigidBody* pusher = world().body(pusherId);
        if (pusher->position.x <= area().position.x + area().size.width - 60) {
            return;
        }
        placeBody(*pusher, pusherStart);
        pusher->velocity = {Speed, 0};
        placeBody(*world().body(targetId), targetStart);
    }

    static constexpr float Speed = 3000;
    Vector2 pusherStart;
    Vector2 targetStart;
    BodyId pusherId = 0;
    BodyId targetId = 0;
};

// Стандарт: упругость считается по скорости до шага, отскок равен удару без накачки.
class ExactRestitutionZone final : public Zone
{
public:
    explicit ExactRestitutionZone(Rect area)
        : Zone(area, "EXACT REBOUND")
    {
        world().gravity = {0, 900};
        addZoneFloor(world(), area);
        start = {zoneCenter(area).x, area.position.y + 80};
        RigidBody ball{Circle{{0, 0}, 14}, BodyType::Dynamic, start};
        ball.restitution = 1;
        ball.friction = 0.05F;
        id = world().addBody(ball);
    }

    std::string state() const override
    {
        return "IMPACT " + formatFloat(impact, 0) + "   REBOUND " + formatFloat(rebound, 0);
    }

    std::string counters() const override
    {
        return "BOUNCES " + std::to_string(bounces) + "   NO GAIN";
    }

private:
    void beforeStep(float deltaTime) override
    {
        dropClock += deltaTime;
        if (dropClock < DropInterval) {
            return;
        }
        dropClock = 0;
        placeBody(*world().body(id), start);
    }

    void afterStep() override
    {
        const RigidBody* body = world().body(id);
        if (previousVy > 100 && body->velocity.y < 0) {
            impact = previousVy;
            rebound = -body->velocity.y;
            ++bounces;
        }
        previousVy = body->velocity.y;
    }

    static constexpr float DropInterval = 4.0F;
    Vector2 start;
    BodyId id = 0;
    float dropClock = 0;
    float previousVy = 0;
    float impact = 0;
    float rebound = 0;
    int bounces = 0;
};

// Стандарт: спящие тела просыпаются от удара целым островом и снова засыпают.
class SleepZone final : public Zone
{
public:
    explicit SleepZone(Rect area)
        : Zone(area, "SLEEPING ISLANDS")
    {
        world().gravity = {0, 900};
        RigidBody floor{Box{{0, 0}, {240, 20}, 0}, BodyType::Static,
            {area.center().x, floorTop(area) + 10}};
        floor.friction = 1;
        world().addBody(floor);
        RigidBody crate{Box{{0, 0}, {CrateWidth, CrateHeight}, 0}, BodyType::Dynamic,
            {area.center().x, floorTop(area) - CrateHeight / 2}};
        crate.friction = 0.8F;
        crateId = world().addBody(crate);
        const Vector2 ballRest{area.center().x, floorTop(area) - CrateHeight - BallRadius};
        RigidBody ball{Circle{{0, 0}, BallRadius}, BodyType::Dynamic, ballRest};
        ball.friction = 0.4F;
        ball.restitution = 0.3F;
        ballId = world().addBody(ball);
    }

    std::string state() const override
    {
        return "SLEEPING " + std::to_string(sleeping) + "/2"
            + (sleeping == 2 ? "   ASLEEP" : "   AWAKE");
    }

    std::string counters() const override
    {
        return "WAKEUPS " + std::to_string(wakeups);
    }

private:
    int sleepingCount() const
    {
        return (world().body(crateId)->sleeping ? 1 : 0)
            + (world().body(ballId)->sleeping ? 1 : 0);
    }

    void beforeStep(float deltaTime) override
    {
        sleeping = sleepingCount();
        if (sleeping == 2) {
            launchClock += deltaTime;
            if (launchClock >= LaunchDelay) {
                launchClock = 0;
                RigidBody* ball = world().body(ballId);
                placeBody(*ball, {area().center().x, area().position.y + 70});
            }
        } else {
            launchClock = 0;
        }
    }

    void afterStep() override
    {
        const int current = sleepingCount();
        if (current < sleeping) {
            ++wakeups;
        }
        sleeping = current;
        markHighlighted(sleeping < 2);
    }

    static constexpr float CrateWidth = 120;
    static constexpr float CrateHeight = 30;
    static constexpr float BallRadius = 10;
    static constexpr float LaunchDelay = 0.8F;
    BodyId crateId = 0;
    BodyId ballId = 0;
    float launchClock = 0;
    int sleeping = 0;
    int wakeups = 0;
};
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

void Zone::drawContent(Renderer2D& drawing) const
{
    const Vector2 origin = zoneArea.position;
    drawing.drawDebugText(zoneTitle, Vector2{zoneArea.center().x, origin.y + TitleOffset}, TitleColor,
        TitlePixel, TextAlignment::Center);
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

void Zone::drawFrame(Renderer2D& drawing) const
{
    DrawOptions frame;
    frame.color = highlight ? ActiveFrameColor : FrameColor;
    frame.lineWidth = highlight ? 3 : 1;
    drawing.drawRectangle(zoneArea, frame, ShapeFill::Outline);
}

namespace
{
std::vector<std::unique_ptr<Zone>> buildInteractionPage()
{
    std::vector<std::unique_ptr<Zone>> zones;
    zones.push_back(std::make_unique<RestitutionZone>(cellArea(0, 0), "ELASTIC BOUNCE", 0.9F));
    zones.push_back(std::make_unique<FrictionSlideZone>(cellArea(1, 0), "FRICTION", 0.9F));
    zones.push_back(std::make_unique<FrictionSlideZone>(cellArea(2, 0), "FRICTIONLESS", 0.0F));
    zones.push_back(std::make_unique<StackZone>(cellArea(0, 1), "STACK", 5, 26));
    zones.push_back(std::make_unique<KinematicCarrierZone>(cellArea(1, 1)));
    zones.push_back(std::make_unique<ForceAtPointZone>(cellArea(2, 1)));
    return zones;
}

std::vector<std::unique_ptr<Zone>> buildEventPage()
{
    std::vector<std::unique_ptr<Zone>> zones;
    zones.push_back(std::make_unique<SensorPassZone>(cellArea(0, 0)));
    zones.push_back(std::make_unique<SensorMaskZone>(cellArea(1, 0)));
    zones.push_back(std::make_unique<SensorRemoveZone>(cellArea(2, 0)));
    zones.push_back(std::make_unique<ImpactImpulseZone>(cellArea(0, 1)));
    zones.push_back(std::make_unique<ContactPointsZone>(cellArea(1, 1)));
    zones.push_back(std::make_unique<ContactPhasesZone>(cellArea(2, 1)));
    return zones;
}

std::vector<std::unique_ptr<Zone>> buildStandardPage()
{
    std::vector<std::unique_ptr<Zone>> zones;
    zones.push_back(std::make_unique<CcdStaticZone>(cellArea(0, 0)));
    zones.push_back(std::make_unique<CcdDynamicZone>(cellArea(1, 0)));
    zones.push_back(std::make_unique<CcdKinematicZone>(cellArea(2, 0)));
    zones.push_back(std::make_unique<ExactRestitutionZone>(cellArea(0, 1)));
    zones.push_back(std::make_unique<StackZone>(cellArea(1, 1), "TALL STACK", 8, 22, 40));
    zones.push_back(std::make_unique<SleepZone>(cellArea(2, 1)));
    return zones;
}
}

void CheckBoard::build()
{
    pages.clear();
    pages.push_back(buildInteractionPage());
    pages.push_back(buildEventPage());
    pages.push_back(buildStandardPage());
    pageNames = {"COLLISIONS AND INTERACTIONS", "EVENTS AND AREAS", "INDUSTRY STANDARD"};
    currentPage = std::min(currentPage, pageCount() - 1);
}

void CheckBoard::update(float deltaTime)
{
    for (const std::unique_ptr<Zone>& zone : pages[static_cast<std::size_t>(currentPage)]) {
        zone->update(deltaTime);
    }
}

void CheckBoard::draw(Renderer2D& drawing, const CameraView& view) const
{
    const std::vector<std::unique_ptr<Zone>>& viewZones = pages[static_cast<std::size_t>(currentPage)];
    // Тело может улететь за пределы зоны: содержимое обрезается по её пиксельной области,
    // а рамки рисуются после содержимого и не обрезаются.
    for (const std::unique_ptr<Zone>& zone : viewZones) {
        drawing.setClip(screenViewport(zone->area(), view));
        zone->drawContent(drawing);
    }
    drawing.setClip(std::nullopt);
    for (const std::unique_ptr<Zone>& zone : viewZones) {
        zone->drawFrame(drawing);
    }
}

void CheckBoard::nextPage()
{
    currentPage = (currentPage + 1) % pageCount();
}

void CheckBoard::showPage(int page)
{
    assert(page >= 0 && page < pageCount());
    currentPage = page;
}

int CheckBoard::pageCount() const
{
    return static_cast<int>(pages.size());
}

const std::string& CheckBoard::pageName() const
{
    return pageNames[static_cast<std::size_t>(currentPage)];
}

const std::vector<std::unique_ptr<Zone>>& CheckBoard::zones() const
{
    return pages[static_cast<std::size_t>(currentPage)];
}
}
