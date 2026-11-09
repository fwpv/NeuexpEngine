#include "entry_point.h"
#include "application.h"
#include "camera.h"
#include "game.h"
#include "geometry.h"
#include "input.h"
#include "math2d.h"
#include "platform.h"
#include "renderer.h"
#include "transform.h"
#include "ui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace Cleanup
{
using Engine::Affine2;
using Engine::Camera2;
using Engine::CameraView;
using Engine::Circle;
using Engine::Color;
using Engine::Rect;
using Engine::Vector2;

constexpr Color OutsideColor = {0.025F, 0.035F, 0.055F};
constexpr Color GridColor = {0.16F, 0.22F, 0.23F};
constexpr Color BorderColor = {0.46F, 0.65F, 0.62F};
constexpr Color OrangePoolColor = {0.76F, 0.25F, 0.035F};
constexpr Color StickyPlayerColor = {1, 0.65F, 0.28F};
constexpr Color CleanPlayerColor = {0.40F, 0.86F, 1};
constexpr Color WhiteColor = {0.95F, 0.98F, 1};
constexpr Color PanelColor = {0.04F, 0.06F, 0.09F};
constexpr Color GoalColor = {0.35F, 0.95F, 0.55F};
constexpr Color WarningColor = {1, 0.25F, 0.20F};
constexpr Color ExhaustedColor = {0.40F, 0.44F, 0.49F};
constexpr std::array<Color, 3> TrashColors = {
    Color{0.94F, 0.84F, 0.43F}, Color{0.75F, 0.55F, 0.85F}, Color{0.67F, 0.78F, 0.81F}
};

Color playerColor(Coat coat)
{
    switch (coat) {
    case Coat::Sticky:
        return StickyPlayerColor;
    case Coat::Clean:
        return CleanPlayerColor;
    default:
        return Color{0.84F, 0.78F, 0.97F};
    }
}

class CleanupApplication final : public Engine::Application
{
private:
    void drawHome(const GameState& game)
    {
        const bool ready = game.washed >= game.rules.homeGoal;
        const float progress = std::min(1.0F, static_cast<float>(game.washed) / static_cast<float>(game.rules.homeGoal));
        drawing.drawRectangle(game.world.homeLotBounds, Color{0.14F + 0.03F * progress, 0.16F + 0.16F * progress, 0.14F});
        drawing.drawRectangle(game.world.homeLotBounds, ready ? GoalColor : BorderColor, Engine::ShapeFill::Outline);
        drawing.drawRectangle(game.world.homeBounds, BorderColor, Engine::ShapeFill::Outline);
        constexpr Color HouseColor = {0.58F, 0.44F, 0.28F};
        if (game.washed >= game.rules.homeGoal / 4) {
            drawing.drawRectangle(Rect{2570, 1810, 260, 24}, HouseColor);
        }
        if (game.washed >= game.rules.homeGoal / 2) {
            drawing.drawRectangle(game.world.homeBounds, HouseColor);
            drawing.drawRectangle(Rect{2610, 1680, 45, 45}, CleanPlayerColor);
            drawing.drawRectangle(Rect{2745, 1680, 45, 45}, CleanPlayerColor);
        }
        if (game.washed >= game.rules.homeGoal * 3 / 4) {
            drawing.drawRectangle(Rect{2560, 1620, 280, 30}, OrangePoolColor);
            drawing.drawRectangle(Rect{2600, 1600, 200, 20}, OrangePoolColor);
        }
        drawing.drawRectangle(Rect{2676, 1760, 48, 70}, ready ? GoalColor : WarningColor);
        drawing.drawDebugText(ready ? "HOME OPEN" : "HOME LOCKED", Vector2{2560, 1870}, ready ? GoalColor : WhiteColor, 3);
        drawing.drawRectangle(Rect{2580, 1910, game.world.homeBounds.size.width, 9}, PanelColor);
        drawing.drawRectangle(Rect{2580, 1910, game.world.homeBounds.size.width * progress, 9}, GoalColor);
    }

    void drawWorld(const GameState& game, float time)
    {
        drawing.drawRectangle(game.world.bounds, Color{0.10F, 0.15F, 0.16F});
        Engine::DrawOptions gridOptions;
        gridOptions.color = GridColor;
        constexpr float GridStep = 100;
        for (float x = GridStep; x < game.world.bounds.size.width; x += GridStep) {
            drawing.drawLine(Vector2{x, 0}, Vector2{x, game.world.bounds.size.height}, gridOptions);
        }
        for (float y = GridStep; y < game.world.bounds.size.height; y += GridStep) {
            drawing.drawLine(Vector2{0, y}, Vector2{game.world.bounds.size.width, y}, gridOptions);
        }
        drawing.drawRectangle(game.world.bounds, BorderColor, Engine::ShapeFill::Outline);
        drawHome(game);
        for (const Rect& obstacle : game.world.obstacles) {
            drawing.drawRectangle(obstacle, Color{0.26F, 0.31F, 0.34F});
            drawing.drawRectangle(obstacle, Color{0.49F, 0.54F, 0.56F}, Engine::ShapeFill::Outline);
        }
        for (const Puddle& puddle : game.world.puddles) {
            drawing.drawCircle(puddle.shape, puddle.sticky ? OrangePoolColor : Color{0.035F, 0.28F, 0.68F}, Engine::ShapeFill::Filled, 32);
            drawing.drawCircle(Circle{puddle.shape.center, puddle.shape.radius - 12}, puddle.sticky ? StickyPlayerColor : CleanPlayerColor, Engine::ShapeFill::Outline, 32);
        }
        if (game.target) {
            drawing.drawCircle(Circle{*game.target, 14}, WhiteColor, Engine::ShapeFill::Outline, 32);
            drawing.drawCircle(Circle{*game.target, 4}, WhiteColor, Engine::ShapeFill::Filled, 32);
        }

        const bool warning = game.carried > 0 && game.cargoAge >= game.rules.glueWarningTime && game.outcome == Outcome::Playing;
        const bool blink = warning && std::sin(time * 12.0f) > 0;
        const Color bodyColor = game.outcome == Outcome::Lost ? ExhaustedColor : playerColor(game.coat);
        drawing.drawCircle(Circle{game.player, game.rules.playerRadius + 3}, blink ? WarningColor : WhiteColor, Engine::ShapeFill::Filled, 32);
        drawing.drawCircle(Circle{game.player, game.rules.playerRadius}, bodyColor, Engine::ShapeFill::Filled, 32);
        const Affine2 playerTransform = Engine::Transform2{game.player, game.heading, Vector2{1, 1}, Vector2{}}.toAffine();
        drawing.drawCircle(Circle{playerTransform.transformPoint(Vector2{8, -6}), 3}, PanelColor, Engine::ShapeFill::Filled, 32);
        drawing.drawCircle(Circle{playerTransform.transformPoint(Vector2{8, 6}), 3}, PanelColor, Engine::ShapeFill::Filled, 32);
        for (std::size_t index = 0; index < game.trash.size(); ++index) {
            const Trash& item = game.trash[index];
            if (item.state == TrashState::Washed) {
                continue;
            }
            Vector2 position = item.state == TrashState::Carried
                ? playerTransform.transformPoint(item.attachment) : item.position;
            if (warning && item.state == TrashState::Carried) {
                position.x += std::sin(time * 25.0f) * 2.0f;
            }
            const Color color = TrashColors[index % TrashColors.size()];
            if (index % 3 == 0) {
                drawing.drawCircle(Circle{position, game.rules.trashRadius}, color, Engine::ShapeFill::Filled, 32);
            } else {
                const float width = index % 3 == 1 ? game.rules.trashRadius * 2 : game.rules.trashRadius;
                drawing.drawRectangle(Rect{position.x - width / 2, position.y - game.rules.trashRadius, width, game.rules.trashRadius * 2}, color);
            }
        }
    }

    bool onStart(Platform::Error& error) override
    {
        return initializeRenderer(error);
    }

    bool updateViews(const Engine::FrameInfo& frame, Platform::Error& error)
    {
        const GameState& state = game.state();
        viewsReady = false;
        if (frame.window.width <= 0 || frame.window.height <= 0) {
            return true;
        }
        const int mapWidth = std::clamp(frame.window.width / 4, 180, 300);
        const int mapHeight = mapWidth * 7 / 10;
        constexpr int Margin = 16;
        const int mapX = frame.window.width - mapWidth - Margin;
        const int mapY = frame.window.height - mapHeight - Margin;
        mapPanel = Rect{static_cast<float>(mapX - 7), static_cast<float>(mapY - 7),
            static_cast<float>(mapWidth + 2 * 7), static_cast<float>(mapHeight + 2 * 7)};
        camera.position = state.player;
        Engine::CameraError cameraError;
        const Engine::Size2 screenSize = {static_cast<float>(frame.window.width), static_cast<float>(frame.window.height)};
        const Camera2 panelCamera = {Vector2{screenSize.width / 2, screenSize.height / 2}, 1, screenSize, Engine::ViewportMode::Expand};
        if (!camera.createView(Engine::Viewport{0, 0, frame.window.width, frame.window.height}, mainView, cameraError)) {
            error = Platform::Error{"Camera2::createView",
                "Could not calculate the main view. CameraError: " + std::to_string(static_cast<int>(cameraError)), std::nullopt};
            return false;
        }
        if (!minimap.createView(Engine::Viewport{mapX, mapY, mapWidth, mapHeight}, mapView, cameraError)) {
            error = Platform::Error{"Camera2::createView",
                "Could not calculate the minimap view. CameraError: " + std::to_string(static_cast<int>(cameraError)), std::nullopt};
            return false;
        }
        if (!panelCamera.createView(Engine::Viewport{0, 0, frame.window.width, frame.window.height}, panelView, cameraError)) {
            error = Platform::Error{"Camera2::createView",
                "Could not calculate the panel view. CameraError: " + std::to_string(static_cast<int>(cameraError)), std::nullopt};
            return false;
        }
        Engine::UiError uiError;
        if (!Engine::UiCanvas{}.createView(Engine::Viewport{0, 0, frame.window.width, frame.window.height}, hudView, uiError)) {
            error = Platform::Error{"UiCanvas::createView",
                "Could not calculate the interface view. UiError: " + std::to_string(static_cast<int>(uiError)), std::nullopt};
            return false;
        }
        viewsReady = true;
        return true;
    }

    bool onInput(const Engine::FrameInfo& frame, Platform::Error& error) override
    {
        const GameState& state = game.state();
        const auto& input = Platform::getInput();
        if (input.key(Engine::Key::Escape).pressed) {
            requestExit();
        }
        if (input.key(Engine::Key::R).pressed) {
            game = Game{};
            setPaused(false);
            camera.zoom = 1;
            titleDirty = true;
        }
        if (input.key(Engine::Key::P).pressed && state.outcome == Outcome::Playing) {
            setPaused(!isPaused());
            titleDirty = true;
        }
        if (input.key(Engine::Key::F).pressed) {
            camera.mode = camera.mode == Engine::ViewportMode::Expand ? Engine::ViewportMode::Fit : Engine::ViewportMode::Expand;
            titleDirty = true;
        }
        const auto& mouse = input.mouse();
        if (mouse.inside) {
            camera.zoom = std::clamp(camera.zoom + static_cast<float>(mouse.wheelY) * 0.1F, 0.5F, 2.0F);
        }
        if (!updateViews(frame, error)) {
            return false;
        }
        if (viewsReady && state.outcome == Outcome::Playing) {
            for (const Engine::Event& event : Platform::getEvents()) {
                if (event.type == Engine::EventType::MousePressed && event.button == Engine::MouseButton::Left) {
                    const Vector2 screen = {static_cast<float>(event.x), static_cast<float>(event.y)};
                    Vector2 world;
                    const bool onMap = Engine::contains(mapPanel, screen);
                    if ((onMap ? mapView : mainView).screenToWorld(screen, world)) {
                        game.setTarget(world);
                    }
                }
            }
        }
        return true;
    }

    bool onUpdate(const Engine::FrameInfo& frame, Platform::Error&) override
    {
        const GameState& state = game.state();
        Vector2 direction;
        if (Platform::isKeyDown(Engine::Key::A) || Platform::isKeyDown(Engine::Key::ArrowLeft)) {
            direction.x -= 1;
        }
        if (Platform::isKeyDown(Engine::Key::D) || Platform::isKeyDown(Engine::Key::ArrowRight)) {
            direction.x += 1;
        }
        if (Platform::isKeyDown(Engine::Key::W) || Platform::isKeyDown(Engine::Key::ArrowUp)) {
            direction.y -= 1;
        }
        if (Platform::isKeyDown(Engine::Key::S) || Platform::isKeyDown(Engine::Key::ArrowDown)) {
            direction.y += 1;
        }
        const Coat previousCoat = state.coat;
        const int previousCarried = state.carried;
        const int previousWashed = state.washed;
        const int previousMotivation = state.motivation;
        const Outcome previousOutcome = state.outcome;
        game.update(direction, frame.deltaTime);
        titleDirty = titleDirty || previousCoat != state.coat || previousCarried != state.carried
            || previousWashed != state.washed || previousMotivation != state.motivation || previousOutcome != state.outcome;
        return true;
    }

    bool onPrepareFrame(const Engine::FrameInfo& frame, Platform::Error& error) override
    {
        const GameState& state = game.state();
        if (!updateViews(frame, error)) {
            return false;
        }
        if (titleDirty || frame.fpsUpdated) {
            const std::string_view coat = state.coat == Coat::Sticky ? "sticky" : (state.coat == Coat::Clean ? "clean" : "dry");
            const std::string_view outcome = state.outcome == Outcome::Won ? "won" : (state.outcome == Outcome::Lost ? "lost" : "playing");
            std::ostringstream title;
            title << "Cleanup | coat=" << coat << " carry=" << state.carried << " washed=" << state.washed << '/' << state.trash.size()
                << " motivation=" << state.motivation << " glue=" << state.glue
                << " age=" << static_cast<int>(state.cargoAge) << " state=" << outcome
                << (state.washed >= state.rules.homeGoal ? " | HOME OPEN" : " | HOME LOCKED") << " goal=" << state.rules.homeGoal
                << (isPaused() ? " | PAUSED" : "")
                << " | WASD/click, wheel: zoom, P: pause, F: "
                << (camera.mode == Engine::ViewportMode::Fit ? "fit" : "expand") << ", R: restart";
            if (!Platform::setWindowTitle(title.str(), error)) {
                return false;
            }
            titleDirty = false;
        }
        return true;
    }

    void renderView(const CameraView& view, bool map, float time)
    {
        const GameState& state = game.state();
        drawing.setWorldView(view);
        const Rect& bounds = view.worldBounds;
        drawing.drawRectangle(bounds, OutsideColor);
        drawWorld(state, time);
        if (map) {
            Rect visible;
            if (Engine::intersection(mainView.worldBounds, state.world.bounds, visible)) {
                drawing.drawRectangle(visible, WhiteColor, Engine::ShapeFill::Outline);
            }
            // Метки остаются различимыми и на маленькой миникарте.
            const float unitsPerPixel = bounds.size.width / static_cast<float>(view.viewport.width);
            for (std::size_t index = 0; index < state.trash.size(); ++index) {
                if (state.trash[index].state == TrashState::Ground) {
                    drawing.drawCircle(Circle{state.trash[index].position, 2 * unitsPerPixel}, TrashColors[index % TrashColors.size()], Engine::ShapeFill::Filled, 32);
                }
            }
            drawing.drawCircle(Circle{state.player, 4 * unitsPerPixel}, WhiteColor, Engine::ShapeFill::Filled, 32);
            drawing.drawCircle(Circle{state.player, 3 * unitsPerPixel}, state.outcome == Outcome::Lost ? ExhaustedColor : playerColor(state.coat), Engine::ShapeFill::Filled, 32);
        }
    }

    bool onRender(const Engine::FrameInfo& frame, Platform::Error& error) override
    {
        const GameState& state = game.state();
        if (!viewsReady) {
            return true;
        }
        beginFrame(frame.window.width, frame.window.height, OutsideColor);
        const float time = static_cast<float>(frame.elapsedTime);
        renderView(mainView, false, time);
        drawing.setWorldView(panelView);
        drawing.drawRectangle(mapPanel, PanelColor);
        drawing.drawRectangle(mapPanel, BorderColor, Engine::ShapeFill::Outline);
        renderView(mapView, true, time);
        drawing.setUiView(hudView);
        const Vector2 homeMarker = mapView.worldToScreen(state.world.homeBounds.center());
        drawing.drawDebugText("HOME", homeMarker + Vector2{-12, 8}, state.washed >= state.rules.homeGoal ? GoalColor : WhiteColor, 1);
        const Color moodColor = state.motivation <= 20 ? WarningColor : GoalColor;
        drawing.drawRectangle(Rect{16, 16, 200, 68}, PanelColor);
        drawing.drawDebugText("MOTIVATION", Vector2{28, 25}, WhiteColor, 1);
        drawing.drawDebugText("-1/" + std::to_string(static_cast<int>(state.rules.motivationDecayInterval)) + "S TO " + std::to_string(state.rules.passiveMotivationFloor), Vector2{28, 48}, BorderColor, 1);
        drawing.drawDebugText(std::to_string(state.motivation), Vector2{166, 44}, moodColor, 2);
        drawing.drawRectangle(Rect{28, 65, 176, 9}, GridColor);
        drawing.drawRectangle(Rect{28, 65, 176 * static_cast<float>(state.motivation) / static_cast<float>(state.rules.maximumMotivation), 9}, moodColor);

        drawing.drawRectangle(Rect{224, 16, 88, 68}, PanelColor);
        drawing.drawDebugText("CARGO", Vector2{236, 25}, WhiteColor, 1);
        drawing.drawDebugText(std::to_string(state.carried), Vector2{236, 44}, StickyPlayerColor, 2);
        drawing.drawCircle(Circle{Vector2{291, 51}, 9}, state.outcome == Outcome::Lost ? ExhaustedColor : playerColor(state.coat), Engine::ShapeFill::Filled, 32);

        const bool homeReady = state.washed >= state.rules.homeGoal;
        const float homeProgress = std::min(1.0F, static_cast<float>(state.washed) / static_cast<float>(state.rules.homeGoal));
        drawing.drawRectangle(Rect{320, 16, 160, 68}, PanelColor);
        drawing.drawDebugText(homeReady ? "HOME OPEN" : "HOME MATERIAL", Vector2{332, 25}, homeReady ? GoalColor : WhiteColor, 1);
        const std::string delivered = std::to_string(state.washed);
        drawing.drawDebugText(delivered, Vector2{332, 44}, CleanPlayerColor, 2);
        const float goalX = 332 + Engine::Renderer2D::measureDebugText(delivered, 2).width + 8;
        drawing.drawDebugText("/ " + std::to_string(state.rules.homeGoal), Vector2{goalX, 44}, WhiteColor, 2);
        drawing.drawRectangle(Rect{332, 65, 136, 6}, GridColor);
        drawing.drawRectangle(Rect{332, 65, 136 * homeProgress, 6}, GoalColor);

        const float bottom = static_cast<float>(frame.window.height);
        drawing.drawRectangle(Rect{16, bottom - 118, 360, 102}, PanelColor);
        drawing.drawDebugText("GLUE", Vector2{28, bottom - 108}, StickyPlayerColor, 1);
        for (int index = 0; index < state.rules.glueCapacity; ++index) {
            drawing.drawRectangle(Rect{86 + static_cast<float>(index) * 17, bottom - 108, 13, 10}, index < state.glue ? StickyPlayerColor : GridColor);
        }
        const bool aging = state.carried > 0 && state.cargoAge < state.rules.glueLifetime;
        if (aging) {
            drawing.drawDebugText("WASH IN " + std::to_string(static_cast<int>(std::ceil(state.rules.glueLifetime - state.cargoAge))) + " S", Vector2{28, bottom - 86}, state.cargoAge >= state.rules.glueWarningTime ? WarningColor : WhiteColor, 1);
        } else {
            drawing.drawDebugText(state.coat != Coat::Sticky ? "VISIT ORANGE" : (state.glue == 0 || state.cargoAge >= state.rules.glueLifetime ? "WASH FIRST" : "COLLECT TRASH"), Vector2{28, bottom - 86}, WhiteColor, 1);
        }
        drawing.drawDebugText("LOST ITEM -" + std::to_string(state.rules.dropPenalty) + "  DELIVERED ITEM +" + std::to_string(state.rules.deliveryReward), Vector2{28, bottom - 64}, BorderColor, 1);
        drawing.drawDebugText("ORANGE - TRASH - BLUE - HOME", Vector2{28, bottom - 42}, WhiteColor, 1);

        if (state.feedbackTime > 0) {
            drawing.drawRectangle(Rect{16, 94, 360, 28}, PanelColor);
            const std::string message = state.motivationChange < 0 ? "LOST CARGO " + std::to_string(state.motivationChange)
                : "DELIVERED " + std::to_string(state.lastDelivery) + "  +" + std::to_string(state.motivationChange);
            drawing.drawDebugText(message, Vector2{28, 103}, state.motivationChange < 0 ? WarningColor : GoalColor, 1);
        } else if (homeReady) {
            drawing.drawRectangle(Rect{16, 94, 360, 28}, PanelColor);
            drawing.drawDebugText("HOME IS OPEN - GO TO GREEN DOOR", Vector2{28, 103}, GoalColor, 1);
        }

        if (state.outcome != Outcome::Playing || isPaused()) {
            const Vector2 center = {static_cast<float>(frame.window.width) / 2, bottom / 2};
            const bool won = state.outcome == Outcome::Won;
            const std::string_view heading = won ? "HOME AT LAST" : (state.outcome == Outcome::Lost ? "NO MOTIVATION" : "PAUSED");
            const Color color = won ? GoalColor : (state.outcome == Outcome::Lost ? WarningColor : WhiteColor);
            drawing.drawRectangle(Rect{center.x - 190, center.y - 65, 380, 130}, PanelColor);
            drawing.drawRectangle(Rect{center.x - 190, center.y - 65, 380, 130}, color, Engine::ShapeFill::Outline);
            const auto headingSize = Engine::Renderer2D::measureDebugText(heading, 2);
            drawing.drawDebugText(heading, Vector2{std::round(center.x - headingSize.width / 2),
                std::round(center.y - 38)}, color, 2);
            if (state.outcome == Outcome::Lost) {
                const auto messageSize = Engine::Renderer2D::measureDebugText("REST AND TRY AGAIN", 1);
                drawing.drawDebugText("REST AND TRY AGAIN", Vector2{std::round(center.x - messageSize.width / 2),
                    std::round(center.y - 4)}, WhiteColor, 1);
            }
            const std::string_view action = state.outcome == Outcome::Playing ? "P TO CONTINUE" : "R TO RESTART";
            const auto actionSize = Engine::Renderer2D::measureDebugText(action, 1);
            drawing.drawDebugText(action, Vector2{std::round(center.x - actionSize.width / 2),
                std::round(center.y + 25)}, WhiteColor, 1);
        }
        return endFrame(error);
    }

    Engine::Renderer2D& drawing = getRenderer();
    CameraView panelView;
    Engine::UiView hudView;
    Game game;
    Camera2 camera = {game.state().world.bounds.center(), 1, Engine::Size2{960, 600}, Engine::ViewportMode::Expand};
    Camera2 minimap = {game.state().world.bounds.center(), 1, game.state().world.bounds.size, Engine::ViewportMode::Fit};
    CameraView mainView;
    CameraView mapView;
    Rect mapPanel;
    bool viewsReady = false;
    bool titleDirty = true;
};
}

int Engine::applicationMain()
{
    Engine::ApplicationOptions options;
    options.window.title = "Cleanup";
    options.window.width = 1100;
    options.window.height = 720;
    options.window.minimumWidth = 640;
    options.window.minimumHeight = 420;
    Cleanup::CleanupApplication application;
    Platform::Error failure;
    const bool success = application.run(options, failure);
    if (!success) {
        Platform::reportError(failure);
    }
    return success ? 0 : 1;
}
