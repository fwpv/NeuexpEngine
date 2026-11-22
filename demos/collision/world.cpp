#include "world.h"
#include "collision/query.h"
#include "collision/shape.h"
#include "core/transform.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace CollisionDemo
{
namespace
{
using namespace NeuexpEngine;

const Color FrameColor{0.35F, 0.4F, 0.5F, 1};
const Color ActiveFrameColor{1, 0.55F, 0.35F, 1};
const Color TitleColor{0.75F, 0.85F, 0.95F, 1};
const Color TextColor{0.85F, 0.87F, 0.92F, 1};
const Color HintColor{0.95F, 0.8F, 0.4F, 1};
const Color CircleColor{0.35F, 0.8F, 1, 1};
const Color RectColor{1, 0.65F, 0.3F, 1};
const Color BoxColor{0.45F, 0.9F, 0.5F, 1};
const Color TriggerColor{0.72F, 0.55F, 1, 1};
const Color MeasureColor{1, 0.85F, 0.3F, 1};
const Color ContactColor{1, 0.45F, 0.35F, 1};

// Подписи внутри зоны стоят под заголовком, а формы занимают остальную часть зоны.
constexpr float TitlePixel = 2;
constexpr float TextPixel = 1;
constexpr float TitleOffset = 10;
constexpr float StateOffset = 30;
constexpr float CountersOffset = 46;
constexpr float ShapesOffset = 45;

Vector2 zoneCenter(const Rect& area)
{
    return {area.center().x, area.center().y + ShapesOffset};
}

std::string formatFloat(float value, int precision)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

Color withAlpha(Color color, float alpha)
{
    color.alpha = alpha;
    return color;
}

Color kindColor(CollisionShape::Kind kind)
{
    if (kind == CollisionShape::Kind::Circle) {
        return CircleColor;
    }
    if (kind == CollisionShape::Kind::Rect) {
        return RectColor;
    }
    return BoxColor;
}

// Центр формы: по нему ставятся отметки контактов и мера расстояния.
Vector2 shapeCenter(const CollisionShape& shape)
{
    if (shape.kind() == CollisionShape::Kind::Circle) {
        return shape.circle().center;
    }
    if (shape.kind() == CollisionShape::Kind::Rect) {
        return shape.rect().center();
    }
    return shape.box().center;
}

// Добавляет форму зоны и возвращает объект: позицию задают правила зоны.
GameObject* addShape(Scene& scene, const CollisionShape& shape, Vector2 position, float rotation = 0,
    CollisionLayer layer = ShapeLayer)
{
    auto object = std::make_unique<GameObject>();
    object->transform().position = position;
    object->transform().rotation = rotation;
    object->collider() = Collider{shape};
    object->collider().layer = layer;
    GameObject* result = object.get();
    scene.add(std::move(object));
    return result;
}

void drawShape(Renderer2D& drawing, const CollisionShape& shape, const ShapeStyle& style)
{
    DrawOptions filled;
    filled.color = style.fill;
    DrawOptions outline;
    outline.color = style.outline;
    outline.lineWidth = 2;
    if (shape.kind() == CollisionShape::Kind::Circle) {
        drawing.drawCircle(shape.circle(), filled, ShapeFill::Filled);
        drawing.drawCircle(shape.circle(), outline, ShapeFill::Outline);
        return;
    }
    if (shape.kind() == CollisionShape::Kind::Rect) {
        drawing.drawRectangle(shape.rect(), filled, ShapeFill::Filled);
        drawing.drawRectangle(shape.rect(), outline, ShapeFill::Outline);
        return;
    }
    drawing.drawPolygon(shape.box().corners(), filled, ShapeFill::Filled);
    drawing.drawPolygon(shape.box().corners(), outline, ShapeFill::Outline);
}

// Пара форм: первая стоит в середине зоны, вторая медленно проходит сквозь неё.
class PairZone final : public ContactZone
{
public:
    PairZone(Rect area, std::string title, const CollisionShape& first, float firstRotation,
        const CollisionShape& second, float secondRotation = 0)
        : ContactZone(area, std::move(title))
    {
        const Vector2 center = zoneCenter(area);
        addShape(scene(), first, Vector2{center.x - 55, center.y}, firstRotation);
        moving = addShape(scene(), second, center, secondRotation);
        centerX = center.x;
        centerY = center.y;
    }

    std::string state() const override
    {
        if (contacts().empty()) {
            return "NO CONTACT";
        }
        const Manifold& manifold = contacts().front().manifold;
        return "CONTACT  depth " + formatFloat(manifold.depth, 1) + "  normal "
            + formatFloat(manifold.normal.x, 2) + "," + formatFloat(manifold.normal.y, 2);
    }

private:
    void move() override
    {
        // Полный проход через зону занимает около шести секунд: за этим можно следить.
        moving->transform().position = Vector2{centerX + 105 * std::sin(elapsedSeconds()), centerY};
    }

    GameObject* moving = nullptr;
    float centerX = 0;
    float centerY = 0;
};

// Триггерная зона: контакт сообщается, но движение объектов не меняется.
class TriggerZone final : public ContactZone
{
public:
    TriggerZone(Rect area, std::string title)
        : ContactZone(area, std::move(title))
    {
        const Vector2 center = zoneCenter(area);
        areaObject = addShape(scene(), Circle{Vector2{}, 70}, center);
        areaObject->collider().trigger = true;
        walker = addShape(scene(), Circle{Vector2{}, 22}, center);
        centerX = center.x;
        centerY = center.y;
    }

    std::string state() const override
    {
        const std::string mode = areaObject->collider().trigger ? "TRIGGER ON (T)" : "TRIGGER OFF (T)";
        return (contacts().empty() ? "CLEAR  " : "CONTACT, NO PUSH  ") + mode;
    }

    bool toggleTrigger() override
    {
        areaObject->collider().trigger = !areaObject->collider().trigger;
        return true;
    }

private:
    void move() override
    {
        walker->transform().position = Vector2{centerX + 110 * std::sin(elapsedSeconds() * 0.9F), centerY};
    }

    ShapeStyle styleOf(const GameObject& object) const override
    {
        if (&object == areaObject) {
            return ShapeStyle{withAlpha(TriggerColor, 0.10F), TriggerColor};
        }
        return ContactZone::styleOf(object);
    }

    GameObject* areaObject = nullptr;
    GameObject* walker = nullptr;
    float centerX = 0;
    float centerY = 0;
};

// Маска пары: формы пересекаются, но маска плиты исключает пару из проверки.
class MaskZone final : public ContactZone
{
public:
    MaskZone(Rect area, std::string title)
        : ContactZone(area, std::move(title))
    {
        const Vector2 center = zoneCenter(area);
        orb = addShape(scene(), Circle{Vector2{}, 30}, Vector2{center.x - 25, center.y});
        slab = addShape(scene(), Rect{-45, -27, 90, 54}, Vector2{center.x + 25, center.y}, 0, SlabLayer);
    }

    std::string state() const override
    {
        const std::string mode = slabsSelfOnly ? "MASK SLABS ONLY (L)" : "MASK ALL (L)";
        return (contacts().empty() ? "PAIR EXCLUDED  " : "CONTACT ALLOWED  ") + mode;
    }

    bool toggleMask() override
    {
        slabsSelfOnly = !slabsSelfOnly;
        slab->collider().mask = slabsSelfOnly ? SlabLayer : AllCollisionLayers;
        return true;
    }

private:
    void move() override {}

    GameObject* orb = nullptr;
    GameObject* slab = nullptr;
    bool slabsSelfOnly = false;
};

// Расстояние между границами: значение падает и растёт, пока формы сближаются.
class DistanceZone final : public Zone
{
public:
    DistanceZone(Rect area, std::string title)
        : Zone(area, std::move(title))
    {
        const Vector2 center = zoneCenter(area);
        first = addShape(scene(), Circle{Vector2{}, 18}, Vector2{center.x - 100, center.y});
        second = addShape(scene(), Circle{Vector2{}, 18}, center);
        centerX = center.x;
        centerY = center.y;
    }

    std::string state() const override
    {
        return "DISTANCE  " + formatFloat(gap(), 1);
    }

private:
    float gap() const
    {
        return distance(first->collider().worldShape(first->transform()),
            second->collider().worldShape(second->transform()));
    }

    void move() override
    {
        second->transform().position = Vector2{centerX + 60 * std::sin(elapsedSeconds() * 0.9F), centerY};
    }

    void drawExtra(Renderer2D& drawing) const override
    {
        const Circle firstCircle = first->collider().worldShape(first->transform()).circle();
        const Circle secondCircle = second->collider().worldShape(second->transform()).circle();
        const Vector2 direction = normalized(secondCircle.center - firstCircle.center);
        const Vector2 from = firstCircle.center + direction * firstCircle.radius;
        const Vector2 to = secondCircle.center - direction * secondCircle.radius;
        DrawOptions options;
        options.color = MeasureColor;
        options.lineWidth = 2;
        drawing.drawLine(from, to, options);
        drawing.drawDebugText(formatFloat(gap(), 1), (from + to) * 0.5F + Vector2{-10, -18}, MeasureColor,
            TextPixel);
    }

    GameObject* first = nullptr;
    GameObject* second = nullptr;
    float centerX = 0;
    float centerY = 0;
};

// Луч: программный отрезок от левой границы зоны к правой, цель проходит поперёк него.
class RayZone final : public Zone
{
public:
    RayZone(Rect area, std::string title, const CollisionShape& targetShape, float targetRotation)
        : Zone(area, std::move(title))
    {
        const Vector2 center = zoneCenter(area);
        target = addShape(scene(), targetShape, center, targetRotation);
        rayBegin = Vector2{center.x - 150, center.y};
        rayEnd = Vector2{center.x + 150, center.y};
        centerX = center.x;
        centerY = center.y;
    }

    std::string state() const override
    {
        if (!hit) {
            return "RAY: MISS";
        }
        return "RAY: HIT  f=" + formatFloat(segmentHit.fraction, 2) + "  at "
            + formatFloat(segmentHit.point.x, 0) + "," + formatFloat(segmentHit.point.y, 0);
    }

private:
    void move() override
    {
        target->transform().position = Vector2{centerX, centerY + 50 * std::sin(elapsedSeconds() * 0.9F)};
        hit = castSegment(target->collider().worldShape(target->transform()), rayBegin, rayEnd, segmentHit);
    }

    void drawExtra(Renderer2D& drawing) const override
    {
        DrawOptions options;
        options.color = hit ? MeasureColor : withAlpha(MeasureColor, 0.4F);
        options.lineWidth = 1;
        drawing.drawLine(rayBegin, rayEnd, options);
        if (!hit) {
            return;
        }
        options.lineWidth = 2;
        drawing.drawCircle(Circle{segmentHit.point, 4}, options, ShapeFill::Outline);
        drawing.drawLine(segmentHit.point, segmentHit.point + segmentHit.normal * 26, options);
    }

    GameObject* target = nullptr;
    Vector2 rayBegin;
    Vector2 rayEnd;
    float centerX = 0;
    float centerY = 0;
    SegmentHit segmentHit;
    bool hit = false;
};

// Точка: проверка попадания точки в повёрнутый прямоугольник.
class PointZone final : public Zone
{
public:
    PointZone(Rect area, std::string title)
        : Zone(area, std::move(title))
    {
        const Vector2 center = zoneCenter(area);
        box = addShape(scene(), Box{Vector2{}, Size2{92, 54}, 0}, center, degreesToRadians(25));
        centerX = center.x;
        centerY = center.y;
        point = center;
    }

    std::string state() const override
    {
        return inside ? "POINT INSIDE THE BOX" : "POINT OUTSIDE";
    }

private:
    void move() override
    {
        const float time = elapsedSeconds();
        point = Vector2{centerX + 70 * std::sin(time * 0.8F), centerY + 45 * std::sin(time * 1.3F)};
        inside = contains(box->collider().worldShape(box->transform()), point);
    }

    void drawExtra(Renderer2D& drawing) const override
    {
        DrawOptions options;
        options.color = inside ? ContactColor : MeasureColor;
        options.lineWidth = 2;
        drawing.drawCircle(Circle{point, 5}, options, ShapeFill::Filled);
    }

    GameObject* box = nullptr;
    Vector2 point;
    float centerX = 0;
    float centerY = 0;
    bool inside = false;
};

std::vector<std::unique_ptr<Zone>> buildPairPage()
{
    const Size2 rectSize{92, 48};
    const Size2 boxSize{92, 40};
    const float boxRotation = degreesToRadians(25);
    const float otherRotation = degreesToRadians(-15);
    std::vector<std::unique_ptr<Zone>> zones;
    zones.push_back(std::make_unique<PairZone>(cellArea(0, 0), "CIRCLE / CIRCLE",
        Circle{Vector2{}, 26}, 0, Circle{Vector2{}, 26}));
    zones.push_back(std::make_unique<PairZone>(cellArea(1, 0), "CIRCLE / RECT",
        Rect{-46, -24, rectSize.width, rectSize.height}, 0, Circle{Vector2{}, 26}));
    zones.push_back(std::make_unique<PairZone>(cellArea(2, 0), "CIRCLE / TURNED BOX",
        Box{Vector2{}, boxSize, 0}, boxRotation, Circle{Vector2{}, 26}));
    zones.push_back(std::make_unique<PairZone>(cellArea(0, 1), "RECT / RECT",
        Rect{-46, -24, rectSize.width, rectSize.height}, 0, Rect{-46, -24, rectSize.width, rectSize.height}));
    zones.push_back(std::make_unique<PairZone>(cellArea(1, 1), "RECT / TURNED BOX",
        Box{Vector2{}, boxSize, 0}, boxRotation, Rect{-46, -24, rectSize.width, rectSize.height}));
    zones.push_back(std::make_unique<PairZone>(cellArea(2, 1), "TURNED BOX / TURNED BOX",
        Box{Vector2{}, boxSize, 0}, boxRotation, Box{Vector2{}, boxSize, 0}, otherRotation));
    return zones;
}

std::vector<std::unique_ptr<Zone>> buildQueryPage()
{
    std::vector<std::unique_ptr<Zone>> zones;
    zones.push_back(std::make_unique<TriggerZone>(cellArea(0, 0), "TRIGGER AREA"));
    zones.push_back(std::make_unique<MaskZone>(cellArea(1, 0), "LAYER MASK"));
    zones.push_back(std::make_unique<DistanceZone>(cellArea(2, 0), "DISTANCE"));
    zones.push_back(std::make_unique<RayZone>(cellArea(0, 1), "RAY / CIRCLE", Circle{Vector2{}, 30}, 0));
    zones.push_back(std::make_unique<RayZone>(cellArea(1, 1), "RAY / TURNED BOX",
        Box{Vector2{}, Size2{92, 40}, 0}, degreesToRadians(20)));
    zones.push_back(std::make_unique<PointZone>(cellArea(2, 1), "POINT IN BOX"));
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
    move();
}

ShapeStyle Zone::styleOf(const GameObject& object) const
{
    const Color color = kindColor(object.collider().shape.kind());
    return ShapeStyle{withAlpha(color, 0.25F), color};
}

void Zone::draw(Renderer2D& drawing) const
{
    DrawOptions frame;
    frame.color = contactActive ? ActiveFrameColor : FrameColor;
    frame.lineWidth = contactActive ? 3 : 1;
    drawing.drawRectangle(zoneArea, frame, ShapeFill::Outline);

    const Vector2 origin = zoneArea.position;
    drawing.drawDebugText(zoneTitle, Vector2{origin.x + 12, origin.y + TitleOffset}, TitleColor, TitlePixel);
    for (const GameObject* object : ownedScene.findAll()) {
        drawShape(drawing, object->collider().worldShape(object->transform()), styleOf(*object));
    }
    drawExtra(drawing);
    drawing.drawDebugText(state(), Vector2{origin.x + 12, origin.y + StateOffset}, TextColor, TextPixel);
    const std::string counterText = counters();
    if (!counterText.empty()) {
        drawing.drawDebugText(counterText, Vector2{origin.x + 12, origin.y + CountersOffset}, HintColor,
            TextPixel);
    }
}

ContactZone::ContactZone(Rect area, std::string title)
    : Zone(area, std::move(title))
{
}

void ContactZone::update(float deltaTime)
{
    Zone::update(deltaTime);
    scene().update(deltaTime);
    const std::vector<ContactEvent>& events = tracker.update(scene());
    currentContacts.clear();
    for (const ContactEvent& event : events) {
        if (event.phase == ContactPhase::Begin) {
            ++begun;
        } else if (event.phase == ContactPhase::End) {
            ++ended;
        }
        if (event.phase != ContactPhase::End) {
            currentContacts.push_back(event.contact);
        }
    }
    markContactActive(!currentContacts.empty());
}

std::string ContactZone::counters() const
{
    return "BEGIN " + std::to_string(begun) + "   END " + std::to_string(ended);
}

void ContactZone::drawExtra(Renderer2D& drawing) const
{
    for (const Contact& contact : currentContacts) {
        const GameObject* first = scene().find(contact.first);
        const GameObject* second = scene().find(contact.second);
        if (!first || !second) {
            continue;
        }
        const Vector2 middle = (shapeCenter(first->collider().worldShape(first->transform()))
            + shapeCenter(second->collider().worldShape(second->transform())))
            * 0.5F;
        DrawOptions options;
        options.color = contact.trigger ? TriggerColor : ContactColor;
        options.lineWidth = 2;
        const float depth = std::min(contact.manifold.depth, 30.0F);
        drawing.drawLine(middle, middle + contact.manifold.normal * depth, options);
        drawing.drawCircle(Circle{middle, 3}, options, ShapeFill::Filled);
    }
}

CheckBoard::CheckBoard()
{
    pageNames = {"PAIR CONTACTS", "RULES AND QUERIES"};
    build();
}

void CheckBoard::build()
{
    pages.clear();
    pages.push_back(buildPairPage());
    pages.push_back(buildQueryPage());
    currentPage = 0;
}

void CheckBoard::restart()
{
    build();
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

const std::vector<std::unique_ptr<Zone>>& CheckBoard::zones() const
{
    return pages[static_cast<std::size_t>(currentPage)];
}

void CheckBoard::toggleTrigger()
{
    for (const std::unique_ptr<Zone>& zone : pages[static_cast<std::size_t>(currentPage)]) {
        zone->toggleTrigger();
    }
}

void CheckBoard::toggleMask()
{
    for (const std::unique_ptr<Zone>& zone : pages[static_cast<std::size_t>(currentPage)]) {
        zone->toggleMask();
    }
}
}
