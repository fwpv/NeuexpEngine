#include "app/application.h"
#include "core/camera.h"
#include "app/entry_point.h"
#include "core/geometry.h"
#include "app/input.h"
#include "core/math2d.h"
#include "scene/object.h"
#include "app/platform.h"
#include "graphics/renderer.h"
#include "core/resources.h"
#include "scene/scene.h"
#include "graphics/sprite.h"
#include "core/texture.h"
#include "core/transform.h"
#include "graphics/ui_view.h"
#include "world.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace NE = NeuexpEngine;

namespace
{
constexpr char WindowTitle[] = "Scenes / objects";
constexpr int WindowWidth = 800;
constexpr int WindowHeight = 600;

const NE::Color Background{0.03F, 0.05F, 0.09F, 1};
const NE::Color TitleColor{0.55F, 0.85F, 0.95F, 1};
const NE::Color TextColor{0.88F, 0.9F, 0.94F, 1};
const NE::Color HintColor{0.95F, 0.8F, 0.4F, 1};
const NE::Color FocusColor{1, 0.85F, 0.3F, 1};

using Populate = void (*)(NE::Scene&, const NE::Texture&, SceneDemo::Statistics&);

// Как найден выбранный объект: по имени или обратным поиском по щелчку.
enum class SelectionKind
{
    None,
    Name,
    Click
};

// Цифровые клавиши соответствуют номерам объектов второй сцены.
constexpr std::array<NE::Key, 10> DigitKeys{
    NE::Key::Digit0, NE::Key::Digit1, NE::Key::Digit2, NE::Key::Digit3, NE::Key::Digit4,
    NE::Key::Digit5, NE::Key::Digit6, NE::Key::Digit7, NE::Key::Digit8, NE::Key::Digit9};

// Сцена демо вместе с подписями для HUD и счётчиками, которые видит пользователь.
struct World
{
    World(const char* title, const char* hiddenGroup, const char* leaderName, Populate populate)
        : title(title)
        , hiddenGroup(hiddenGroup)
        , leaderName(leaderName)
        , populate(populate)
    {
    }

    NE::Scene scene;
    std::string title;
    // Группа, видимость которой переключается клавишей H.
    std::string hiddenGroup;
    // Объект, который обновление сцены находит по имени каждый кадр; пусто — нет такого.
    std::string leaderName;
    Populate populate = nullptr;
    SceneDemo::Statistics statistics;
    // Выбранный объект и способ поиска: их показывают HUD и обводка.
    NE::ObjectId selection = 0;
    SelectionKind selectionKind = SelectionKind::None;
    int restarts = 0;
};

// Приложение владеет двумя сценами и общей для них текстурой: смена активной
// сцены не освобождает кадры и не оставляет ссылок на удалённые объекты.
class SceneApplication final : public NE::Application
{
private:
    bool onStart(Platform::Error& error) override
    {
        if (!initializeRenderer(error)) {
            return false;
        }
        NE::ResourceError resourceError;
        if (!getRenderer().createTexture(SceneDemo::makeArtImage(), art, resourceError)) {
            error = Platform::Error{"Renderer2D::createTexture", resourceError.message, std::nullopt};
            return false;
        }
        rebuild(orbit);
        rebuild(grid);
        return true;
    }

    bool onInput(const NE::FrameInfo& frame, Platform::Error&) override
    {
        if (Platform::isKeyDown(Platform::Key::Escape)) {
            requestExit();
        }
        if (Platform::isKeyPressed(Platform::Key::Tab)) {
            activeWorld = 1 - activeWorld;
        }
        if (Platform::isKeyPressed(Platform::Key::R)) {
            rebuild(active());
            ++active().restarts;
        }
        if (Platform::isKeyPressed(Platform::Key::H)) {
            toggleHidden(active());
        }
        selectByNumber(active());
        selectByClick(active(), frame);
        return true;
    }

    bool onUpdate(const NE::FrameInfo& frame, Platform::Error&) override
    {
        World& world = active();
        world.scene.update(frame.deltaTime);
        // Первая сцена сама находит охотника по имени: выбор обновляется каждый кадр.
        if (!world.leaderName.empty()) {
            const NE::GameObject* leader = world.scene.findByName(world.leaderName);
            world.selection = leader ? leader->id() : 0;
            world.selectionKind = leader ? SelectionKind::Name : SelectionKind::None;
        }
        return true;
    }

    bool onRender(const NE::FrameInfo& frame, Platform::Error& error) override
    {
        World& world = active();
        NE::CameraView worldView;
        NE::CameraError cameraError;
        if (!world.scene.createView(frame.window.width, frame.window.height, worldView, cameraError)) {
            error = Platform::Error{"Scene::createView", "Could not calculate the world view. CameraError: "
                + std::to_string(static_cast<int>(cameraError)), std::nullopt};
            return false;
        }
        NE::UiView hudView;
        NE::UiError uiError;
        if (!NE::UiCanvas{}.createView(NE::Viewport{0, 0, frame.window.width, frame.window.height}, hudView, uiError)) {
            error = Platform::Error{"UiCanvas::createView", "Could not calculate the interface view. UiError: "
                + std::to_string(static_cast<int>(uiError)), std::nullopt};
            return false;
        }

        NE::Renderer2D& drawing = getRenderer();
        beginFrame(frame.window.width, frame.window.height, Background);
        drawing.setWorldView(worldView);
        world.scene.draw(drawing);
        drawSelection(drawing, world);
        drawing.setUiView(hudView);
        drawHud(drawing, world, activeWorld + 1);
        return endFrame(error);
    }

    World& active()
    {
        return activeWorld == 0 ? orbit : grid;
    }

    void rebuild(World& world)
    {
        world.scene.clear();
        world.statistics = SceneDemo::Statistics{};
        world.selection = 0;
        world.selectionKind = SelectionKind::None;
        world.populate(world.scene, art, world.statistics);
    }

    void toggleHidden(World& world)
    {
        const std::vector<NE::GameObject*> objects = world.scene.findByGroup(world.hiddenGroup);
        const bool visible = objects.empty() || !objects.front()->isVisible();
        for (NE::GameObject* object : objects) {
            object->setVisible(visible);
        }
    }

    // Прямой поиск: цифра выбирает объект по имени. В сцене без такого имени выбор не меняется.
    void selectByNumber(World& world)
    {
        for (std::size_t digit = 0; digit < DigitKeys.size(); ++digit) {
            if (!Platform::isKeyPressed(DigitKeys[digit])) {
                continue;
            }
            const NE::GameObject* object = world.scene.findByName("box-" + std::to_string(digit));
            if (object) {
                world.selection = object->id();
                world.selectionKind = SelectionKind::Name;
            }
        }
    }

    // Обратный поиск: щелчок по объекту показывает его номер. Верхний объект проверяется первым.
    void selectByClick(World& world, const NE::FrameInfo& frame)
    {
        if (!Platform::getInput().button(NE::MouseButton::Left).pressed) {
            return;
        }
        NE::CameraView view;
        NE::CameraError cameraError;
        if (!world.scene.createView(frame.window.width, frame.window.height, view, cameraError)) {
            return;
        }
        const NE::MouseState& mouse = Platform::getInput().mouse();
        NE::Vector2 point;
        if (!view.screenToWorld(NE::Vector2{static_cast<float>(mouse.x), static_cast<float>(mouse.y)}, point)) {
            return;
        }
        const std::vector<NE::GameObject*> objects = world.scene.findByGroup("box");
        for (auto object = objects.rbegin(); object != objects.rend(); ++object) {
            const NE::Quad shape = NE::transformed((*object)->visual().polygon(), (*object)->transform().toAffine());
            if (NE::contains(shape, point)) {
                world.selection = (*object)->id();
                world.selectionKind = SelectionKind::Click;
                return;
            }
        }
    }

    // Найденный объект обводится рамкой: ссылка живёт только внутри кадра и проверяется заново.
    void drawSelection(NE::Renderer2D& drawing, const World& world) const
    {
        const NE::GameObject* object = world.scene.find(world.selection);
        if (!object) {
            return;
        }
        NE::DrawOptions options;
        options.color = FocusColor;
        options.lineWidth = 2;
        drawing.drawCircle(NE::Circle{object->transform().position, 26}, options, NE::ShapeFill::Outline);
    }

    // Имя и идентификатор выбранного объекта с указанием того, каким поиском он найден.
    std::string foundText(const World& world) const
    {
        const NE::GameObject* object = world.scene.find(world.selection);
        if (!object) {
            return "none";
        }
        const char* source = world.selectionKind == SelectionKind::Click ? "click" : "name";
        return object->name() + " #" + std::to_string(object->id()) + " (" + source + ")";
    }

    void drawHud(NE::Renderer2D& drawing, const World& world, int number) const
    {
        const std::size_t live = world.scene.count();
        const std::size_t grouped = world.scene.findByGroup(world.hiddenGroup).size();
        const std::array<std::string, 5> lines{
            "SCENE " + std::to_string(number) + " OF 2: " + world.title,
            "OBJECTS " + std::to_string(live) + "   GROUP " + world.hiddenGroup + " " + std::to_string(grouped),
            "FOUND: " + foundText(world),
            "CAUGHT " + std::to_string(world.statistics.caught) + "   EXPIRED "
                + std::to_string(world.statistics.expired) + "   RESTARTS " + std::to_string(world.restarts)
                + "   TEXTURES " + std::to_string(drawing.textureStats().count),
            "TAB SCENE   R RESTART   H HIDE   DIGITS/CLICK FIND   ESC EXIT"};
        for (std::size_t index = 0; index < lines.size(); ++index) {
            const NE::Color color = index == 0 ? TitleColor : (index + 1 == lines.size() ? HintColor : TextColor);
            drawing.drawDebugText(lines[index], NE::Vector2{18, 16 + static_cast<float>(index) * 20}, color, 1);
        }
    }

    World orbit{"ORBIT", "star", "hunter", &SceneDemo::populateOrbit};
    World grid{"GRID", "box", "", &SceneDemo::populateGrid};
    NE::Texture art;
    int activeWorld = 0;
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
    SceneApplication application;
    Platform::Error failure;
    const bool success = application.run(options, failure);
    if (!success) {
        Platform::reportError(failure);
    }
    return success ? 0 : 1;
}
