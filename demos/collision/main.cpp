#include "app/application.h"
#include "app/entry_point.h"
#include "app/input.h"
#include "app/platform.h"
#include "collision/collider.h"
#include "collision/contacts.h"
#include "collision/query.h"
#include "collision/shape.h"
#include "core/camera.h"
#include "core/geometry.h"
#include "core/math2d.h"
#include "graphics/renderer.h"
#include "graphics/ui_view.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace NE = NeuexpEngine;

namespace
{
constexpr char WindowTitle[] = "Collisions / contacts";
constexpr int WindowWidth = 800;
constexpr int WindowHeight = 600;

const NE::Color Background{0.03F, 0.05F, 0.09F, 1};
const NE::Color TitleColor{0.55F, 0.85F, 0.95F, 1};
const NE::Color TextColor{0.88F, 0.9F, 0.94F, 1};
const NE::Color HintColor{0.95F, 0.8F, 0.4F, 1};
const NE::Color OutlineColor{0.9F, 0.93F, 1, 0.85F};
const NE::Color RayColor{1, 0.85F, 0.3F, 1};
const NE::Color ContactColor{1, 0.45F, 0.35F, 1};
const NE::Color TriggerColor{0.72F, 0.55F, 1, 1};
const NE::Color TriggerFill{0.72F, 0.55F, 1, 0.12F};
const NE::Color DisabledColor{0.6F, 0.6F, 0.65F, 1};

const NE::Color OrbColor{0.35F, 0.8F, 1, 1};
const NE::Color SlabColor{1, 0.65F, 0.3F, 1};
const NE::Color GearColor{0.45F, 0.9F, 0.5F, 1};
const NE::Color WalkerColor{1, 0.9F, 0.4F, 1};

// Начало пробного отрезка мыши: он показывает первую форму на своём пути.
const NE::Vector2 RayOrigin{20, 300};
// Столько контактов HUD перечисляет по именам.
constexpr int ListedContacts = 3;

std::string formatFloat(float value, int precision)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

// Цвет формы по группе объекта: вид формы при этом остаётся узнаваемым сам по себе.
NE::Color colorOf(const NE::GameObject& object)
{
    const std::string& group = object.group();
    if (group == CollisionDemo::OrbGroup) {
        return OrbColor;
    }
    if (group == CollisionDemo::SlabGroup) {
        return SlabColor;
    }
    if (group == CollisionDemo::GearGroup) {
        return GearColor;
    }
    return WalkerColor;
}

// Демонстрация столкновений: сцена с формами, трекер контактов и переключатели,
// которые меняют правила пар на ходу.
class CollisionApplication final : public NE::Application
{
private:
    bool onStart(Platform::Error& error) override
    {
        if (!initializeRenderer(error)) {
            return false;
        }
        rebuild();
        return true;
    }

    bool onInput(const NE::FrameInfo& frame, Platform::Error&) override
    {
        if (Platform::isKeyDown(Platform::Key::Escape)) {
            requestExit();
        }
        if (Platform::isKeyPressed(Platform::Key::P)) {
            setPaused(!isPaused());
        }
        if (Platform::isKeyPressed(Platform::Key::R)) {
            rebuild();
        }
        if (Platform::isKeyPressed(Platform::Key::T)) {
            toggleTrigger();
        }
        if (Platform::isKeyPressed(Platform::Key::L)) {
            toggleLayers();
        }
        updateRay(frame);
        if (Platform::isKeyPressed(Platform::Key::D)) {
            removeUnderCursor();
        }
        return true;
    }

    bool onUpdate(const NE::FrameInfo& frame, Platform::Error&) override
    {
        scene.update(frame.deltaTime);
        collectEvents();
        return true;
    }

    bool onRender(const NE::FrameInfo& frame, Platform::Error& error) override
    {
        NE::CameraView worldView;
        NE::CameraError cameraError;
        if (!scene.createView(frame.window.width, frame.window.height, worldView, cameraError)) {
            error = Platform::Error{"Scene::createView", "Could not calculate the world view. CameraError: "
                + std::to_string(static_cast<int>(cameraError)), std::nullopt};
            return false;
        }
        NE::UiView hudView;
        NE::UiError uiError;
        if (!NE::UiCanvas{}.createView(NE::Viewport{0, 0, frame.window.width, frame.window.height}, hudView,
                uiError)) {
            error = Platform::Error{"UiCanvas::createView", "Could not calculate the interface view. UiError: "
                + std::to_string(static_cast<int>(uiError)), std::nullopt};
            return false;
        }

        NE::Renderer2D& drawing = getRenderer();
        beginFrame(frame.window.width, frame.window.height, Background);
        drawing.setWorldView(worldView);
        drawWorld(drawing);
        drawing.setUiView(hudView);
        drawHud(drawing);
        return endFrame(error);
    }

    // Перезапуск: сцена очищается, поэтому контакты прежних объектов завершаются,
    // а у новых начинаются заново. Идентификаторы продолжают расти.
    void rebuild()
    {
        scene.clear();
        CollisionDemo::populate(scene);
        applyLayerMode();
        begun = 0;
        ended = 0;
        persisted = 0;
        events.clear();
        clearRay();
    }

    // Собирает переходы контактов и накапливает счётчики HUD.
    void collectEvents()
    {
        const std::vector<NE::ContactEvent>& updated = tracker.update(scene);
        events.assign(updated.begin(), updated.end());
        for (const NE::ContactEvent& event : events) {
            if (event.phase == NE::ContactPhase::Begin) {
                ++begun;
            } else if (event.phase == NE::ContactPhase::End) {
                ++ended;
            } else {
                ++persisted;
            }
        }
    }

    void toggleTrigger()
    {
        NE::GameObject* zone = scene.findByName(CollisionDemo::ZoneName);
        if (zone) {
            zone->collider().trigger = !zone->collider().trigger;
        }
    }

    // Маска плит отключает их пары с остальными формами: контакты этой группы
    // завершаются, а пары плит между собой остаются.
    void toggleLayers()
    {
        slabsSelfOnly = !slabsSelfOnly;
        applyLayerMode();
    }

    void applyLayerMode()
    {
        for (NE::GameObject* slab : scene.findByGroup(CollisionDemo::SlabGroup)) {
            slab->collider().mask = slabsSelfOnly ? CollisionDemo::SlabLayer : NE::AllCollisionLayers;
        }
    }

    bool zoneIsTrigger() const
    {
        const NE::GameObject* zone = scene.findByName(CollisionDemo::ZoneName);
        return zone != nullptr && zone->collider().trigger;
    }

    // Пробный отрезок: от неподвижной точки через курсор. Первое попадание по ходу
    // отрезка показывает форма, ближайшая к его началу.
    void updateRay(const NE::FrameInfo& frame)
    {
        clearRay();
        NE::CameraView view;
        NE::CameraError cameraError;
        if (!scene.createView(frame.window.width, frame.window.height, view, cameraError)) {
            return;
        }
        const NE::MouseState& mouse = Platform::getInput().mouse();
        NE::Vector2 point;
        if (!view.screenToWorld(NE::Vector2{static_cast<float>(mouse.x), static_cast<float>(mouse.y)}, point)) {
            return;
        }
        rayEnd = point;
        rayValid = true;
        for (const NE::GameObject* object : scene.findAll()) {
            const NE::Collider& collider = object->collider();
            if (!collider.enabled) {
                continue;
            }
            NE::SegmentHit hit;
            if (!NE::castSegment(collider.worldShape(object->transform()), RayOrigin, point, hit)) {
                continue;
            }
            if (!hasRayHit || hit.fraction < rayHit.fraction) {
                rayHit = hit;
                rayTarget = object->id();
                hasRayHit = true;
            }
        }
    }

    void clearRay()
    {
        rayValid = false;
        hasRayHit = false;
        rayHit = NE::SegmentHit{};
        rayTarget = 0;
    }

    // Удаление формы под курсором: контакт с ней завершается один раз, как и с любым
    // удалённым объектом. Верхний объект проверяется первым, как и рисуется.
    void removeUnderCursor()
    {
        if (!rayValid) {
            return;
        }
        const std::vector<const NE::GameObject*> objects = scene.findAll();
        for (auto object = objects.rbegin(); object != objects.rend(); ++object) {
            const NE::Collider& collider = (*object)->collider();
            if (!collider.enabled) {
                continue;
            }
            if (NE::contains(collider.worldShape((*object)->transform()), rayEnd)) {
                scene.destroy((*object)->id());
                clearRay();
                return;
            }
        }
    }

    void drawWorld(NE::Renderer2D& drawing) const
    {
        for (const NE::GameObject* object : scene.findAll()) {
            if (!object->isVisible()) {
                continue;
            }
            const NE::Collider& collider = object->collider();
            const NE::CollisionShape shape = collider.worldShape(object->transform());
            if (!collider.enabled) {
                drawShape(drawing, shape, DisabledColor, NE::ShapeFill::Outline, 1);
                continue;
            }
            const bool highlighted = hasRayHit && object->id() == rayTarget;
            if (collider.trigger) {
                drawShape(drawing, shape, TriggerFill, NE::ShapeFill::Filled, 1);
                drawShape(drawing, shape, TriggerColor, NE::ShapeFill::Outline, highlighted ? 3.0F : 2.0F);
                continue;
            }
            drawShape(drawing, shape, colorOf(*object), NE::ShapeFill::Filled, 1);
            drawShape(drawing, shape, highlighted ? RayColor : OutlineColor, NE::ShapeFill::Outline,
                highlighted ? 3.0F : 2.0F);
        }
        drawContacts(drawing);
        drawRay(drawing);
    }

    // Форма рисуется по коллайдеру: на экране те же границы, по которым считаются контакты.
    void drawShape(NE::Renderer2D& drawing, const NE::CollisionShape& shape, NE::Color color,
        NE::ShapeFill fill, float lineWidth) const
    {
        NE::DrawOptions options;
        options.color = color;
        options.lineWidth = lineWidth;
        if (shape.kind() == NE::CollisionShape::Kind::Circle) {
            drawing.drawCircle(shape.circle(), options, fill);
            return;
        }
        if (shape.kind() == NE::CollisionShape::Kind::Rect) {
            drawing.drawRectangle(shape.rect(), options, fill);
            return;
        }
        drawing.drawPolygon(shape.box().corners(), options, fill);
    }

    // Отметка контакта: середина между центрами форм и направление выталкивания,
    // длина отрезка пропорциональна глубине проникновения.
    void drawContacts(NE::Renderer2D& drawing) const
    {
        for (const NE::ContactEvent& event : events) {
            if (event.phase == NE::ContactPhase::End) {
                continue;
            }
            const NE::GameObject* first = scene.find(event.contact.first);
            const NE::GameObject* second = scene.find(event.contact.second);
            if (!first || !second) {
                continue;
            }
            const NE::Vector2 firstCenter
                = CollisionDemo::shapeCenter(first->collider().worldShape(first->transform()));
            const NE::Vector2 secondCenter
                = CollisionDemo::shapeCenter(second->collider().worldShape(second->transform()));
            const NE::Vector2 middle = (firstCenter + secondCenter) * 0.5F;
            NE::DrawOptions options;
            options.color = event.contact.trigger ? TriggerColor : ContactColor;
            options.lineWidth = 2;
            const float depth = std::min(event.contact.manifold.depth, 40.0F);
            drawing.drawLine(middle, middle + event.contact.manifold.normal * depth, options);
            drawing.drawCircle(NE::Circle{middle, 2}, options, NE::ShapeFill::Filled);
        }
    }

    // Пробный отрезок и найденное попадание с нормалью формы в точке касания.
    void drawRay(NE::Renderer2D& drawing) const
    {
        if (!rayValid) {
            return;
        }
        NE::DrawOptions options;
        options.color = RayColor;
        options.lineWidth = 1;
        const NE::Vector2 end = hasRayHit ? rayHit.point : rayEnd;
        drawing.drawLine(RayOrigin, end, options);
        if (!hasRayHit) {
            return;
        }
        options.lineWidth = 2;
        drawing.drawCircle(NE::Circle{rayHit.point, 4}, options, NE::ShapeFill::Outline);
        drawing.drawLine(rayHit.point, rayHit.point + rayHit.normal * 26, options);
    }

    std::size_t activeContacts() const
    {
        return static_cast<std::size_t>(std::count_if(events.begin(), events.end(),
            [](const NE::ContactEvent& event) { return event.phase != NE::ContactPhase::End; }));
    }

    std::string nameOf(NE::ObjectId id) const
    {
        const NE::GameObject* object = scene.find(id);
        return object ? object->name() : std::string{"?"};
    }

    // Текущие контакты: имена пары, глубина проникновения и признак триггера.
    std::string contactsText() const
    {
        std::string text = "CONTACTS:";
        int listed = 0;
        for (const NE::ContactEvent& event : events) {
            if (event.phase == NE::ContactPhase::End) {
                continue;
            }
            if (listed == ListedContacts) {
                text += " ...";
                break;
            }
            text += " " + nameOf(event.contact.first) + "/" + nameOf(event.contact.second) + " d"
                + formatFloat(event.contact.manifold.depth, 1);
            if (event.contact.trigger) {
                text += " T";
            }
            ++listed;
        }
        if (listed == 0) {
            text += " none";
        }
        return text;
    }

    std::string rayText() const
    {
        if (!rayValid) {
            return "RAY: cursor outside the world";
        }
        if (!hasRayHit) {
            return "RAY: no hit";
        }
        return "RAY HIT: " + nameOf(rayTarget) + " at " + formatFloat(rayHit.point.x, 1) + ","
            + formatFloat(rayHit.point.y, 1) + " fraction " + formatFloat(rayHit.fraction, 2) + " normal "
            + formatFloat(rayHit.normal.x, 2) + "," + formatFloat(rayHit.normal.y, 2);
    }

    void drawHud(NE::Renderer2D& drawing) const
    {
        const std::string states = std::string{"LAYERS "}
            + (slabsSelfOnly ? "SLABS ONLY" : "ALL") + "   ZONE TRIGGER " + (zoneIsTrigger() ? "ON" : "OFF")
            + (isPaused() ? "   PAUSED" : "");
        const std::array<std::string, 6> lines{
            "COLLISIONS: CIRCLES, BOXES, TURNED BOXES AND TRIGGERS",
            "OBJECTS " + std::to_string(scene.count()) + "   ACTIVE " + std::to_string(activeContacts())
                + "   BEGIN " + std::to_string(begun) + "   PERSIST " + std::to_string(persisted)
                + "   END " + std::to_string(ended),
            contactsText(),
            rayText(),
            states,
            "P PAUSE   R RESTART   L LAYERS   T TRIGGER   D REMOVE UNDER CURSOR   ESC EXIT"};
        for (std::size_t index = 0; index < lines.size(); ++index) {
            const NE::Color color = index == 0 ? TitleColor
                : (index + 1 == lines.size() ? HintColor : TextColor);
            drawing.drawDebugText(lines[index], NE::Vector2{18, 16 + static_cast<float>(index) * 20}, color, 1);
        }
    }

    NE::Scene scene;
    NE::ContactTracker tracker;
    std::vector<NE::ContactEvent> events;
    bool slabsSelfOnly = false;
    int begun = 0;
    int ended = 0;
    int persisted = 0;
    bool rayValid = false;
    bool hasRayHit = false;
    NE::SegmentHit rayHit;
    NE::Vector2 rayEnd;
    NE::ObjectId rayTarget = 0;
};
}

int NeuexpEngine::applicationMain()
{
    NE::ApplicationOptions options;
    options.window.width = WindowWidth;
    options.window.height = WindowHeight;
    options.window.minimumWidth = 320;
    options.window.minimumHeight = 240;
    options.window.resizable = true;
    options.window.title = WindowTitle;
    CollisionApplication application;
    Platform::Error failure;
    const bool success = application.run(options, failure);
    if (!success) {
        Platform::reportError(failure);
    }
    return success ? 0 : 1;
}
