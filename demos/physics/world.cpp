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

// Объект зоны: коллайдер даёт форму и правила пар, тело — динамику.
ObjectId createBody(Scene& scene, const CollisionShape& shape, BodyType type, Vector2 position,
    float angle = 0)
{
    auto object = std::make_unique<GameObject>();
    object->collider() = Collider{shape};
    object->transform().position = position;
    object->transform().rotation = angle;
    PhysicsBody settings;
    settings.type = type;
    object->enablePhysics(settings);
    return scene.add(std::move(object));
}

ObjectId addZoneFloor(Scene& scene, const Rect& area, float friction = 0.6F, float bottomMargin = 55)
{
    const ObjectId id = createBody(scene, Box{{0, 0}, {area.size.width - 120, 20}, 0},
        BodyType::Static, {area.center().x, floorTop(area, bottomMargin) + 10});
    scene.find(id)->body().friction = friction;
    return id;
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

// Перенос объекта приложением: поза меняется, а скорость сбрасывается.
void placeObject(GameObject& object, Vector2 position, float angle = 0)
{
    object.transform().position = position;
    object.transform().rotation = angle;
    object.body().velocity = {};
    object.body().angularVelocity = 0;
    object.body().wake();
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
        addZoneFloor(scene(), area);
        start = {zoneCenter(area).x, area.position.y + 80};
        id = createBody(scene(), Circle{{0, 0}, 14}, BodyType::Dynamic, start);
        GameObject& object = *scene().find(id);
        object.body().restitution = restitution;
        object.body().friction = 0.2F;
    }

    std::string state() const override
    {
        const GameObject* object = scene().find(id);
        if (object->body().sleeping) {
            return "E " + formatFloat(bounce, 2) + "   SETTLED";
        }
        return "E " + formatFloat(bounce, 2) + "   VY " + formatFloat(object->body().velocity.y, 0);
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
        placeObject(*scene().find(id), start);
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
    ObjectId id = 0;
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
        addZoneFloor(scene(), area, friction);
        start = {area.position.x + 55, floorTop(area) - 12};
        id = createBody(scene(), Box{{0, 0}, {40, 24}, 0}, BodyType::Dynamic, start);
        GameObject& object = *scene().find(id);
        object.body().friction = friction;
        object.body().velocity = {StartSpeed, 0};
    }

    std::string state() const override
    {
        return "VX " + formatFloat(scene().find(id)->body().velocity.x, 0);
    }

    std::string counters() const override
    {
        return "FRICTION " + formatFloat(surfaceFriction, 1) + "   SLID "
            + formatFloat(scene().find(id)->transform().position.x - start.x, 0) + "   RUNS "
            + std::to_string(runs);
    }

private:
    void beforeStep(float deltaTime) override
    {
        GameObject& object = *scene().find(id);
        if (std::fabs(object.body().velocity.x) < 1) {
            stopClock += deltaTime;
        } else {
            stopClock = 0;
        }
        const bool escaped = object.transform().position.x > area().position.x + area().size.width - 30;
        if (stopClock > 1.5F || escaped) {
            placeObject(object, start);
            object.body().velocity = {StartSpeed, 0};
            stopClock = 0;
            ++runs;
        }
    }

    static constexpr float StartSpeed = 300;
    float surfaceFriction = 0;
    Vector2 start;
    ObjectId id = 0;
    float stopClock = 0;
    int runs = 0;
};

// Стопка: ящики падают на пол, успокаиваются и стоят неподвижно; затем падают снова.
class StackZone final : public Zone
{
public:
    StackZone(Rect area, std::string title, int levels, float size, float bottomMargin = 55,
        float dropHeight = 40)
        : Zone(area, std::move(title))
        , count(levels)
        , boxSize(size)
        , drop(dropHeight)
    {
        world().gravity = {0, 900};
        const ObjectId floorId = createBody(scene(), Box{{0, 0}, {240, 20}, 0}, BodyType::Static,
            {area.center().x, floorTop(area, bottomMargin) + 10});
        scene().find(floorId)->body().friction = 1;
        const float firstY = floorTop(area, bottomMargin) - boxSize / 2;
        for (int level = 0; level < count; ++level) {
            const float restY = firstY - boxSize * static_cast<float>(level);
            // До падения ящики разделены зазором: они падают по очереди и укладываются вплотную.
            const ObjectId id = createBody(scene(), Box{{0, 0}, {boxSize, boxSize}, 0},
                BodyType::Dynamic, {area.center().x, restY - drop - SpawnGap * static_cast<float>(level)});
            scene().find(id)->body().friction = 0.8F;
            ids.push_back(id);
            expected.push_back(restY);
            dropStart.push_back(scene().find(id)->transform().position);
        }
    }

    std::string state() const override
    {
        float drift = 0;
        float sink = 0;
        float rise = 0;
        for (std::size_t index = 0; index < ids.size(); ++index) {
            const GameObject* object = scene().find(ids[index]);
            drift = std::max(drift, std::fabs(object->transform().position.x - area().center().x));
            const float offset = object->transform().position.y - expected[index];
            sink = std::max(sink, offset);
            rise = std::min(rise, offset);
        }
        const bool stable = drift < 1 && sink < 3;
        return "LEVELS " + std::to_string(count) + "   DRIFT " + formatFloat(drift, 1)
            + "   SINK " + formatFloat(sink, 1)
            + (rise < -2 ? "   FALLING" : (stable ? "   STABLE" : "   SETTLING"));
    }

    std::string counters() const override
    {
        return "FRICTION 0.8   DROPS " + std::to_string(drops);
    }

private:
    void beforeStep(float deltaTime) override
    {
        lastStep = deltaTime;
    }

    void afterStep() override
    {
        bool allSleeping = true;
        for (const ObjectId id : ids) {
            allSleeping = allSleeping && scene().find(id)->body().sleeping;
        }
        if (!allSleeping) {
            stillClock = 0;
            return;
        }
        stillClock += lastStep;
        if (stillClock < RestInterval) {
            return;
        }
        stillClock = 0;
        ++drops;
        for (std::size_t index = 0; index < ids.size(); ++index) {
            placeObject(*scene().find(ids[index]), dropStart[index]);
        }
    }

    int count = 0;
    float boxSize = 0;
    float drop = 0;
    static constexpr float RestInterval = 3.5F;
    static constexpr float SpawnGap = 3;
    std::vector<ObjectId> ids;
    std::vector<float> expected;
    std::vector<Vector2> dropStart;
    float lastStep = 0;
    float stillClock = 0;
    int drops = 0;
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
        platformId = createBody(scene(), Box{{0, 0}, {150, 18}, 0}, BodyType::Kinematic,
            {center.x, center.y + 10});
        scene().find(platformId)->body().velocity = {Speed, 0};
        scene().find(platformId)->body().friction = 0.9F;
        crateStart = {center.x, center.y - 14};
        crateId = createBody(scene(), Box{{0, 0}, {30, 30}, 0}, BodyType::Dynamic, crateStart);
        scene().find(crateId)->body().friction = 0.9F;
    }

    std::string state() const override
    {
        return "CRATE SHIFT "
            + formatFloat(std::fabs(scene().find(crateId)->transform().position.x - crateStart.x), 0);
    }

    std::string counters() const override
    {
        return "PLATFORM VX " + formatFloat(scene().find(platformId)->body().velocity.x, 0);
    }

private:
    void beforeStep(float) override
    {
        GameObject& platform = *scene().find(platformId);
        if (platform.transform().position.x <= area().position.x + 80) {
            platform.body().velocity.x = Speed;
        } else if (platform.transform().position.x >= area().position.x + area().size.width - 80) {
            platform.body().velocity.x = -Speed;
        }
    }

    static constexpr float Speed = 90;
    ObjectId platformId = 0;
    ObjectId crateId = 0;
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
        id = createBody(scene(), Box{{0, 0}, {64, 20}, 0}, BodyType::Dynamic, start);
    }

    std::string state() const override
    {
        const GameObject* object = scene().find(id);
        return "W " + formatFloat(object->body().angularVelocity, 2) + "   VY "
            + formatFloat(object->body().velocity.y, 0);
    }

    std::string counters() const override
    {
        return "FORCE AT X+28";
    }

private:
    void beforeStep(float) override
    {
        GameObject& object = *scene().find(id);
        applyForceAtPoint(object, Vector2{0, Force}, object.transform().position + Vector2{28, 0});
        if (object.transform().position.y > area().position.y + area().size.height - 40) {
            placeObject(object, start);
        }
    }

    static constexpr float Force = 40000;
    Vector2 start;
    ObjectId id = 0;
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
        sensorId = createBody(scene(), Circle{{0, 0}, 65}, BodyType::Static, center);
        scene().find(sensorId)->collider().trigger = true;
        start = {area.position.x + 40, center.y};
        ballId = createBody(scene(), Circle{{0, 0}, 16}, BodyType::Dynamic, start);
        scene().find(ballId)->body().velocity = {Speed, 0};
    }

    std::string state() const override
    {
        return "INSIDE " + std::to_string(inside) + "   SPEED "
            + formatFloat(scene().find(ballId)->body().velocity.x, 0);
    }

    std::string counters() const override
    {
        return "BEGIN " + std::to_string(begun) + "   END " + std::to_string(ended);
    }

private:
    void beforeStep(float) override
    {
        GameObject& ball = *scene().find(ballId);
        if (ball.transform().position.x > area().position.x + area().size.width - 40) {
            placeObject(ball, start);
            ball.body().velocity = {Speed, 0};
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
    ObjectId sensorId = 0;
    ObjectId ballId = 0;
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
        sensorId = createBody(scene(), Circle{{0, 0}, 65}, BodyType::Static, center);
        GameObject& sensor = *scene().find(sensorId);
        sensor.collider().trigger = true;
        sensor.collider().layer = 2U;
        sensor.collider().mask = 1U;
        start = {area.position.x + 40, center.y};
        ballId = createBody(scene(), Circle{{0, 0}, 16}, BodyType::Dynamic, start);
        scene().find(ballId)->body().velocity = {Speed, 0};
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
        return scene().find(sensorId)->collider().mask == 1U;
    }

    void beforeStep(float deltaTime) override
    {
        toggleClock += deltaTime;
        if (toggleClock >= ToggleInterval) {
            toggleClock = 0;
            Collider& sensor = scene().find(sensorId)->collider();
            sensor.mask = allowed() ? 2U : 1U;
        }
        GameObject& ball = *scene().find(ballId);
        if (ball.transform().position.x > area().position.x + area().size.width - 40) {
            placeObject(ball, start);
            ball.body().velocity = {Speed, 0};
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
    ObjectId sensorId = 0;
    ObjectId ballId = 0;
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
        sensorId = createBody(scene(), Box{{0, 0}, {120, 60}, 0}, BodyType::Static, zoneCenter(area));
        scene().find(sensorId)->collider().trigger = true;
        spawnBall();
    }

    std::string state() const override
    {
        return "REMOVED " + std::to_string(removed) + "   ALIVE "
            + std::to_string(scene().count() - 1);
    }

    std::string counters() const override
    {
        return "DEFERRED REMOVAL";
    }

private:
    void spawnBall()
    {
        ballId = createBody(scene(), Circle{{0, 0}, 14}, BodyType::Dynamic, start);
        scene().find(ballId)->body().velocity = {Speed, 0};
    }

    void beforeStep(float deltaTime) override
    {
        if (scene().count() == 1) {
            respawnClock += deltaTime;
            if (respawnClock >= RespawnTime) {
                respawnClock = 0;
                spawnBall();
            }
        }
        GameObject* ball = scene().find(ballId);
        if (ball != nullptr && ball->transform().position.x > area().position.x + area().size.width - 30) {
            placeObject(*ball, start);
        }
    }

    void afterStep() override
    {
        bool hit = false;
        for (const PhysicsContactEvent& event : world().contactEvents()) {
            if (!event.contact.sensor || event.phase != ContactPhase::Begin) {
                continue;
            }
            const ObjectId other = event.contact.first == sensorId
                ? event.contact.second : event.contact.first;
            if (scene().find(other) == nullptr) {
                continue;
            }
            scene().destroy(other);
            ++removed;
            hit = true;
        }
        markHighlighted(hit);
    }

    static constexpr float Speed = 70;
    static constexpr float RespawnTime = 0.6F;
    Vector2 start;
    ObjectId sensorId = 0;
    ObjectId ballId = 0;
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
        addZoneFloor(scene(), area);
        start = {zoneCenter(area).x, area.position.y + 80};
        id = createBody(scene(), Circle{{0, 0}, 14}, BodyType::Dynamic, start);
        scene().find(id)->body().restitution = 0.85F;
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
        placeObject(*scene().find(id), start);
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
    ObjectId id = 0;
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
        addZoneFloor(scene(), area);
        const ObjectId id = createBody(scene(), Box{{0, 0}, {60, 26}, 0}, BodyType::Dynamic,
            {zoneCenter(area).x, floorTop(area) - 13});
        scene().find(id)->body().friction = 0.8F;
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
        const ObjectId sensorId = createBody(scene(), Circle{{0, 0}, 55}, BodyType::Static, center);
        scene().find(sensorId)->collider().trigger = true;
        start = {area.position.x + 50, center.y};
        ballId = createBody(scene(), Circle{{0, 0}, 14}, BodyType::Dynamic, start);
        scene().find(ballId)->body().velocity = {Speed, 0};
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
        GameObject& ball = *scene().find(ballId);
        if (ball.transform().position.x > area().position.x + area().size.width - 40) {
            ball.body().velocity.x = -Speed;
        } else if (ball.transform().position.x < area().position.x + 40) {
            ball.body().velocity.x = Speed;
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
    ObjectId ballId = 0;
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
        wallId = createBody(scene(), Box{{0, 0}, {8, 240}, 0}, BodyType::Static, center);
        start = {area.position.x + 40, center.y};
        ballId = createBody(scene(), Circle{{0, 0}, 10}, BodyType::Dynamic, start);
        scene().find(ballId)->body().velocity = {Speed, 0};
    }

    std::string state() const override
    {
        const GameObject* ball = scene().find(ballId);
        const float gap = (center.x - 4) - (ball->transform().position.x + 10);
        return "VX " + formatFloat(ball->body().velocity.x, 0) + "   GAP " + formatFloat(gap, 1);
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
        placeObject(*scene().find(ballId), start);
        scene().find(ballId)->body().velocity = {Speed, 0};
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
    ObjectId wallId = 0;
    ObjectId ballId = 0;
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
        wallId = createBody(scene(), Box{{0, 0}, {8, 240}, 0}, BodyType::Dynamic, center);
        scene().find(wallId)->body().friction = 0.2F;
        scene().find(wallId)->body().density = 40;
        start = {area.position.x + 40, center.y};
        ballId = createBody(scene(), Circle{{0, 0}, 10}, BodyType::Dynamic, start);
        scene().find(ballId)->body().velocity = {Speed, 0};
    }

    std::string state() const override
    {
        const GameObject* ball = scene().find(ballId);
        const GameObject* wall = scene().find(wallId);
        const float gap = (wall->transform().position.x - 4) - (ball->transform().position.x + 10);
        return "VX " + formatFloat(ball->body().velocity.x, 0) + "   GAP " + formatFloat(gap, 1);
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
        placeObject(*scene().find(wallId), center);
        placeObject(*scene().find(ballId), start);
        scene().find(ballId)->body().velocity = {Speed, 0};
    }

    Color fillOf(const GameObject& object) const override
    {
        if (object.id() == wallId) {
            return StaticFill;
        }
        return Zone::fillOf(object);
    }

    static constexpr float Speed = 5000;
    static constexpr float ResetInterval = 1.6F;
    Vector2 center;
    Vector2 start;
    ObjectId wallId = 0;
    ObjectId ballId = 0;
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
        pusherId = createBody(scene(), Box{{0, 0}, {40, 40}, 0}, BodyType::Kinematic, pusherStart);
        scene().find(pusherId)->body().velocity = {Speed, 0};
        targetStart = {area.position.x + 170, zoneCenter(area).y};
        targetId = createBody(scene(), Circle{{0, 0}, 12}, BodyType::Dynamic, targetStart);
    }

    std::string state() const override
    {
        return "PUSHER X " + formatFloat(scene().find(pusherId)->transform().position.x - pusherStart.x, 0)
            + "   TARGET VX " + formatFloat(scene().find(targetId)->body().velocity.x, 0);
    }

    std::string counters() const override
    {
        return "PUSHER 3000   KINEMATIC";
    }

private:
    void beforeStep(float) override
    {
        GameObject& pusher = *scene().find(pusherId);
        if (pusher.transform().position.x <= area().position.x + area().size.width - 60) {
            return;
        }
        placeObject(pusher, pusherStart);
        pusher.body().velocity = {Speed, 0};
        placeObject(*scene().find(targetId), targetStart);
    }

    static constexpr float Speed = 3000;
    Vector2 pusherStart;
    Vector2 targetStart;
    ObjectId pusherId = 0;
    ObjectId targetId = 0;
};

// Стандарт: упругость считается по скорости до шага, отскок равен удару без накачки.
class ExactRestitutionZone final : public Zone
{
public:
    explicit ExactRestitutionZone(Rect area)
        : Zone(area, "EXACT REBOUND")
    {
        world().gravity = {0, 900};
        addZoneFloor(scene(), area);
        start = {zoneCenter(area).x, area.position.y + 80};
        id = createBody(scene(), Circle{{0, 0}, 14}, BodyType::Dynamic, start);
        scene().find(id)->body().restitution = 1;
        scene().find(id)->body().friction = 0.05F;
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
        placeObject(*scene().find(id), start);
    }

    void afterStep() override
    {
        const PhysicsBody& body = scene().find(id)->body();
        if (previousVy > 100 && body.velocity.y < 0) {
            impact = previousVy;
            rebound = -body.velocity.y;
            ++bounces;
        }
        previousVy = body.velocity.y;
    }

    static constexpr float DropInterval = 4.0F;
    Vector2 start;
    ObjectId id = 0;
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
        const ObjectId floorId = createBody(scene(), Box{{0, 0}, {240, 20}, 0}, BodyType::Static,
            {area.center().x, floorTop(area) + 10});
        scene().find(floorId)->body().friction = 1;
        crateId = createBody(scene(), Box{{0, 0}, {CrateWidth, CrateHeight}, 0}, BodyType::Dynamic,
            {area.center().x, floorTop(area) - CrateHeight / 2});
        scene().find(crateId)->body().friction = 0.8F;
        const Vector2 ballRest{area.center().x, floorTop(area) - CrateHeight - BallRadius};
        ballId = createBody(scene(), Circle{{0, 0}, BallRadius}, BodyType::Dynamic, ballRest);
        scene().find(ballId)->body().friction = 0.4F;
        scene().find(ballId)->body().restitution = 0.3F;
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
        return (scene().find(crateId)->body().sleeping ? 1 : 0)
            + (scene().find(ballId)->body().sleeping ? 1 : 0);
    }

    void beforeStep(float deltaTime) override
    {
        sleeping = sleepingCount();
        if (sleeping == 2) {
            launchClock += deltaTime;
            if (launchClock >= LaunchDelay) {
                launchClock = 0;
                placeObject(*scene().find(ballId), {area().center().x, area().position.y + 70});
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
    ObjectId crateId = 0;
    ObjectId ballId = 0;
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
    ownedScene.update(deltaTime);
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

Color Zone::fillOf(const GameObject& object) const
{
    if (object.collider().trigger) {
        return SensorFill;
    }
    if (object.body().type == BodyType::Static) {
        return StaticFill;
    }
    if (object.body().type == BodyType::Kinematic) {
        return KinematicFill;
    }
    return object.collider().shape.kind() == CollisionShape::Kind::Circle ? BallFill : CrateFill;
}

Color Zone::outlineOf(const GameObject& object) const
{
    if (object.collider().trigger) {
        return SensorOutline;
    }
    if (object.body().type == BodyType::Static) {
        return StaticOutline;
    }
    if (object.body().type == BodyType::Kinematic) {
        return KinematicOutline;
    }
    return object.collider().shape.kind() == CollisionShape::Kind::Circle ? BallOutline : CrateOutline;
}

void Zone::drawContent(Renderer2D& drawing) const
{
    const Vector2 origin = zoneArea.position;
    drawing.drawDebugText(zoneTitle, Vector2{origin.x + 12, origin.y + TitleOffset}, TitleColor,
        TitlePixel);
    for (const GameObject* object : ownedScene.findAll()) {
        drawShape(drawing, object->collider().worldShape(object->transform()), fillOf(*object),
            outlineOf(*object));
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
    zones.push_back(std::make_unique<StackZone>(cellArea(1, 1), "TALL STACK", 8, 20, 40, 25));
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
