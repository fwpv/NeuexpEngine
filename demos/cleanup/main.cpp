#include "application.h"
#include "camera.h"
#include "game.h"
#include "../common/pixel_font.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace Cleanup
{
using Engine::Affine2;
using Engine::Camera2;
using Engine::CameraView;
using Engine::Circle;
using Engine::Color;
using Engine::Rect;
using Engine::Vector2;

constexpr float GridStep = 100;
constexpr int CircleSegments = 32;
constexpr int Margin = 16;
constexpr int MapPadding = 7;

constexpr Color OutsideColor = { 0.025F, 0.035F, 0.055F };
constexpr Color GroundColor = { 0.10F, 0.15F, 0.16F };
constexpr Color GridColor = { 0.16F, 0.22F, 0.23F };
constexpr Color BorderColor = { 0.46F, 0.65F, 0.62F };
constexpr Color OrangePoolColor = { 0.76F, 0.25F, 0.035F };
constexpr Color BluePoolColor = { 0.035F, 0.28F, 0.68F };
constexpr Color DryPlayerColor = { 0.84F, 0.78F, 0.97F };
constexpr Color StickyPlayerColor = { 1, 0.65F, 0.28F };
constexpr Color CleanPlayerColor = { 0.40F, 0.86F, 1 };
constexpr Color WhiteColor = { 0.95F, 0.98F, 1 };
constexpr Color PanelColor = { 0.04F, 0.06F, 0.09F };
constexpr Color GoalColor = { 0.35F, 0.95F, 0.55F };
constexpr Color WarningColor = { 1, 0.25F, 0.20F };
constexpr Color ExhaustedColor = { 0.40F, 0.44F, 0.49F };
constexpr Color HouseColor = { 0.58F, 0.44F, 0.28F };
constexpr Color ObstacleColor = { 0.26F, 0.31F, 0.34F };
constexpr Color ObstacleBorderColor = { 0.49F, 0.54F, 0.56F };
constexpr std::array<Color, 3> TrashColors = {{
    { 0.94F, 0.84F, 0.43F }, { 0.75F, 0.55F, 0.85F }, { 0.67F, 0.78F, 0.81F }
}};

void setColor(Color color)
{
    glColor4f(color.red, color.green, color.blue, color.alpha);
}

void vertex(Vector2 point)
{
    glVertex2f(point.x, point.y);
}

void rectangle(const Rect& bounds, Color color, bool filled = true)
{
    setColor(color);
    glBegin(filled ? GL_QUADS : GL_LINE_LOOP);
    vertex(bounds.position);
    vertex(bounds.position + Vector2{ bounds.size.width, 0 });
    vertex(bounds.position + Vector2{ bounds.size.width, bounds.size.height });
    vertex(bounds.position + Vector2{ 0, bounds.size.height });
    glEnd();
}

void circle(const Circle& shape, Color color, bool filled = true)
{
    setColor(color);
    glBegin(filled ? GL_TRIANGLE_FAN : GL_LINE_LOOP);
    if (filled) {
        vertex(shape.center);
    }
    for (int index = 0; index <= CircleSegments; ++index) {
        const float angle = 2 * Engine::Pi * static_cast<float>(index) / CircleSegments;
        vertex(shape.center + Engine::rotated({ shape.radius, 0 }, angle));
    }
    glEnd();
}

Color playerColor(Coat coat)
{
    switch (coat) {
    case Coat::Sticky:
        return StickyPlayerColor;
    case Coat::Clean:
        return CleanPlayerColor;
    default:
        return DryPlayerColor;
    }
}

using Demo::text;

void number(int value, Vector2 position, Color color)
{
    text(std::to_string(value), position, color, 6);
}

void drawHome(const Game& game)
{
    const bool ready = game.washed >= HomeGoal;
    const float progress = std::min(1.0F, static_cast<float>(game.washed) / HomeGoal);
    rectangle(HomeLotBounds, { 0.14F + 0.03F * progress, 0.16F + 0.16F * progress, 0.14F });
    rectangle(HomeLotBounds, ready ? GoalColor : BorderColor, false);
    rectangle(HomeBounds, BorderColor, false);
    if (game.washed >= HomeGoal / 4) {
        rectangle({ 2570, 1810, 260, 24 }, HouseColor);
    }
    if (game.washed >= HomeGoal / 2) {
        rectangle(HomeBounds, HouseColor);
        rectangle({ 2610, 1680, 45, 45 }, CleanPlayerColor);
        rectangle({ 2745, 1680, 45, 45 }, CleanPlayerColor);
    }
    if (game.washed >= HomeGoal * 3 / 4) {
        rectangle({ 2560, 1620, 280, 30 }, OrangePoolColor);
        rectangle({ 2600, 1600, 200, 20 }, OrangePoolColor);
    }
    rectangle({ 2676, 1760, 48, 70 }, ready ? GoalColor : WarningColor);
    text(ready ? "HOME OPEN" : "HOME LOCKED", { 2560, 1870 }, ready ? GoalColor : WhiteColor, 4);
    rectangle({ 2580, 1910, HomeBounds.size.width, 9 }, PanelColor);
    rectangle({ 2580, 1910, HomeBounds.size.width * progress, 9 }, GoalColor);
}

void drawWorld(const Game& game, float time)
{
    rectangle(WorldBounds, GroundColor);
    setColor(GridColor);
    glBegin(GL_LINES);
    for (float x = GridStep; x < WorldBounds.size.width; x += GridStep) {
        vertex({ x, 0 });
        vertex({ x, WorldBounds.size.height });
    }
    for (float y = GridStep; y < WorldBounds.size.height; y += GridStep) {
        vertex({ 0, y });
        vertex({ WorldBounds.size.width, y });
    }
    glEnd();
    rectangle(WorldBounds, BorderColor, false);
    drawHome(game);
    for (const Rect& obstacle : Obstacles) {
        rectangle(obstacle, ObstacleColor);
        rectangle(obstacle, ObstacleBorderColor, false);
    }
    for (const Puddle& puddle : Puddles) {
        circle(puddle.shape, puddle.sticky ? OrangePoolColor : BluePoolColor);
        circle({ puddle.shape.center, puddle.shape.radius - 12 }, puddle.sticky ? StickyPlayerColor : CleanPlayerColor, false);
    }
    if (game.target) {
        circle({ *game.target, 14 }, WhiteColor, false);
        circle({ *game.target, 4 }, WhiteColor);
    }

    const bool warning = game.carried > 0 && game.cargoAge >= GlueWarningTime && game.outcome == Outcome::Playing;
    const bool blink = warning && std::sin(time * 12) > 0;
    const Color bodyColor = game.outcome == Outcome::Lost ? ExhaustedColor : playerColor(game.coat);
    circle({ game.player, PlayerRadius + 3 }, blink ? WarningColor : WhiteColor);
    circle({ game.player, PlayerRadius }, bodyColor);
    const Affine2 playerTransform = Engine::Transform2{ game.player, game.heading, { 1, 1 }, {} }.toAffine();
    circle({ playerTransform.transformPoint({ 8, -6 }), 3 }, PanelColor);
    circle({ playerTransform.transformPoint({ 8, 6 }), 3 }, PanelColor);
    for (std::size_t index = 0; index < game.trash.size(); ++index) {
        const Trash& item = game.trash[index];
        if (item.state == TrashState::Washed) {
            continue;
        }
        Vector2 position = item.state == TrashState::Carried
            ? playerTransform.transformPoint(item.attachment) : item.position;
        if (warning && item.state == TrashState::Carried) {
            position.x += std::sin(time * 25) * 2;
        }
        const Color color = TrashColors[index % TrashColors.size()];
        if (index % 3 == 0) {
            circle({ position, TrashRadius }, color);
        } else {
            const float width = index % 3 == 1 ? TrashRadius * 2 : TrashRadius;
            rectangle({ position.x - width / 2, position.y - TrashRadius, width, TrashRadius * 2 }, color);
        }
    }
}

class CleanupApplication final : public Engine::Application
{
private:
    Game game;
    Camera2 camera = { WorldBounds.center(), 1, { 960, 600 }, Engine::ViewportMode::Expand };
    Camera2 minimap = { WorldBounds.center(), 1, WorldBounds.size, Engine::ViewportMode::Fit };
    CameraView mainView;
    CameraView mapView;
    Rect mapPanel;
    bool viewsReady = false;
    bool titleDirty = true;

    bool updateViews(const Engine::FrameInfo& frame, Platform::Error& error)
    {
        viewsReady = false;
        if (frame.window.width <= 0 || frame.window.height <= 0) {
            return true;
        }
        const int mapWidth = std::clamp(frame.window.width / 4, 180, 300);
        const int mapHeight = mapWidth * 7 / 10;
        const int mapX = frame.window.width - mapWidth - Margin;
        const int mapY = frame.window.height - mapHeight - Margin;
        mapPanel = { static_cast<float>(mapX - MapPadding), static_cast<float>(mapY - MapPadding),
            static_cast<float>(mapWidth + 2 * MapPadding), static_cast<float>(mapHeight + 2 * MapPadding) };
        camera.position = game.player;
        Engine::CameraError cameraError;
        if (!camera.createView({ 0, 0, frame.window.width, frame.window.height }, mainView, cameraError)
            || !minimap.createView({ mapX, mapY, mapWidth, mapHeight }, mapView, cameraError)) {
            error = { "Camera2::createView", "Could not calculate the camera viewport.", {} };
            return false;
        }
        viewsReady = true;
        return true;
    }

    bool onInput(const Engine::FrameInfo& frame, Platform::Error& error) override
    {
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
        if (input.key(Engine::Key::P).pressed && game.outcome == Outcome::Playing) {
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
        if (viewsReady && game.outcome == Outcome::Playing) {
            for (const Engine::Event& event : Platform::getEvents()) {
                if (event.type == Engine::EventType::MousePressed && event.button == Engine::MouseButton::Left) {
                    const Vector2 screen = { static_cast<float>(event.x), static_cast<float>(event.y) };
                    Vector2 world;
                    const bool onMap = Engine::contains(mapPanel, screen);
                    if ((onMap ? mapView : mainView).screenToWorld(screen, world)) {
                        game.target = Engine::closestPoint({ PlayerRadius, PlayerRadius,
                            WorldBounds.size.width - 2 * PlayerRadius, WorldBounds.size.height - 2 * PlayerRadius }, world);
                    }
                }
            }
        }
        return true;
    }

    bool onUpdate(const Engine::FrameInfo& frame, Platform::Error&) override
    {
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
        const Coat previousCoat = game.coat;
        const int previousCarried = game.carried;
        const int previousWashed = game.washed;
        const int previousMotivation = game.motivation;
        const Outcome previousOutcome = game.outcome;
        game.update(direction, frame.deltaTime);
        titleDirty = titleDirty || previousCoat != game.coat || previousCarried != game.carried
            || previousWashed != game.washed || previousMotivation != game.motivation || previousOutcome != game.outcome;
        return true;
    }

    bool onPrepareFrame(const Engine::FrameInfo& frame, Platform::Error& error) override
    {
        if (!updateViews(frame, error)) {
            return false;
        }
        if (titleDirty || frame.fpsUpdated) {
            const char* coat = game.coat == Coat::Sticky ? "sticky" : (game.coat == Coat::Clean ? "clean" : "dry");
            const char* state = game.outcome == Outcome::Won ? "won" : (game.outcome == Outcome::Lost ? "lost" : "playing");
            std::ostringstream title;
            title << "Cleanup | coat=" << coat << " carry=" << game.carried << " washed=" << game.washed << '/' << TrashCount
                << " motivation=" << game.motivation << " glue=" << game.glue
                << " age=" << static_cast<int>(game.cargoAge) << " state=" << state
                << (game.washed >= HomeGoal ? " | HOME OPEN" : " | HOME LOCKED") << " goal=" << HomeGoal
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

    void renderView(const CameraView& view, int windowHeight, bool map, float time)
    {
        const auto& viewport = view.viewport;
        glViewport(viewport.x, windowHeight - viewport.y - viewport.height, viewport.width, viewport.height);
        glScissor(viewport.x, windowHeight - viewport.y - viewport.height, viewport.width, viewport.height);
        glEnable(GL_SCISSOR_TEST);
        glClearColor(OutsideColor.red, OutsideColor.green, OutsideColor.blue, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        const Rect& bounds = view.worldBounds;
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(bounds.position.x, bounds.position.x + bounds.size.width,
            bounds.position.y + bounds.size.height, bounds.position.y, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        drawWorld(game, time);
        if (map) {
            Rect visible;
            if (Engine::intersection(mainView.worldBounds, WorldBounds, visible)) {
                rectangle(visible, WhiteColor, false);
            }
            // Метки остаются различимыми и на маленькой миникарте.
            const float unitsPerPixel = bounds.size.width / static_cast<float>(viewport.width);
            for (std::size_t index = 0; index < game.trash.size(); ++index) {
                if (game.trash[index].state == TrashState::Ground) {
                    circle({ game.trash[index].position, 2 * unitsPerPixel }, TrashColors[index % TrashColors.size()]);
                }
            }
            circle({ game.player, 4 * unitsPerPixel }, WhiteColor);
            circle({ game.player, 3 * unitsPerPixel }, game.outcome == Outcome::Lost ? ExhaustedColor : playerColor(game.coat));
        }
        glDisable(GL_SCISSOR_TEST);
    }

    void screenCoordinates(const Engine::FrameInfo& frame)
    {
        glViewport(0, 0, frame.window.width, frame.window.height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, frame.window.width, frame.window.height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    bool onRender(const Engine::FrameInfo& frame, Platform::Error&) override
    {
        if (!viewsReady) {
            return true;
        }
        glDisable(GL_DEPTH_TEST);
        glClearColor(OutsideColor.red, OutsideColor.green, OutsideColor.blue, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        const float time = static_cast<float>(frame.elapsedTime);
        renderView(mainView, frame.window.height, false, time);
        screenCoordinates(frame);
        rectangle(mapPanel, PanelColor);
        rectangle(mapPanel, BorderColor, false);
        renderView(mapView, frame.window.height, true, time);
        screenCoordinates(frame);
        const Vector2 homeMarker = mapView.worldToScreen(HomeBounds.center());
        text("HOME", homeMarker + Vector2{ -12, 8 }, game.washed >= HomeGoal ? GoalColor : WhiteColor, 1.5F);
        const Color moodColor = game.motivation <= 20 ? WarningColor : GoalColor;
        rectangle({ 16, 16, 235, 78 }, PanelColor);
        text("MOTIVATION", { 28, 25 }, WhiteColor);
        text("-1/" + std::to_string(static_cast<int>(MotivationDecayInterval)) + "S TO " + std::to_string(PassiveMotivationFloor),
            { 28, 45 }, BorderColor);
        number(game.motivation, { 170, 44 }, moodColor);
        rectangle({ 28, 65, 130, 9 }, GridColor);
        rectangle({ 28, 65, 130 * static_cast<float>(game.motivation) / MaximumMotivation, 9 }, moodColor);

        rectangle({ 260, 16, 116, 78 }, PanelColor);
        text("CARGO", { 272, 25 }, WhiteColor);
        number(game.carried, { 272, 44 }, StickyPlayerColor);
        circle({ { 349, 61 }, 13 }, game.outcome == Outcome::Lost ? ExhaustedColor : playerColor(game.coat));

        const bool homeReady = game.washed >= HomeGoal;
        const float homeProgress = std::min(1.0F, static_cast<float>(game.washed) / HomeGoal);
        rectangle({ 385, 16, 235, 78 }, PanelColor);
        text(homeReady ? "HOME OPEN" : "HOME MATERIAL", { 397, 25 }, homeReady ? GoalColor : WhiteColor);
        number(game.washed, { 397, 44 }, CleanPlayerColor);
        text("/", { 451, 49 }, WhiteColor, 4);
        number(HomeGoal, { 475, 44 }, WhiteColor);
        rectangle({ 397, 82, 211, 6 }, GridColor);
        rectangle({ 397, 82, 211 * homeProgress, 6 }, GoalColor);

        const float bottom = static_cast<float>(frame.window.height);
        rectangle({ 16, bottom - 118, 360, 102 }, PanelColor);
        text("GLUE", { 28, bottom - 108 }, StickyPlayerColor);
        for (int index = 0; index < GlueCapacity; ++index) {
            rectangle({ 86 + static_cast<float>(index) * 17, bottom - 108, 13, 10 },
                index < game.glue ? StickyPlayerColor : GridColor);
        }
        const bool aging = game.carried > 0 && game.cargoAge < GlueLifetime;
        if (aging) {
            text("WASH IN " + std::to_string(static_cast<int>(std::ceil(GlueLifetime - game.cargoAge))) + " S",
                { 28, bottom - 86 }, game.cargoAge >= GlueWarningTime ? WarningColor : WhiteColor);
        } else {
            text(game.coat != Coat::Sticky ? "VISIT ORANGE" : (game.glue == 0 || game.cargoAge >= GlueLifetime ? "WASH FIRST" : "COLLECT TRASH"),
                { 28, bottom - 86 }, WhiteColor);
        }
        text("LOST ITEM -" + std::to_string(DropPenalty) + "  DELIVERED ITEM +" + std::to_string(DeliveryReward),
            { 28, bottom - 64 }, BorderColor);
        text("ORANGE - TRASH - BLUE - HOME", { 28, bottom - 42 }, WhiteColor);

        if (game.feedbackTime > 0) {
            rectangle({ 16, 104, 360, 28 }, PanelColor);
            const std::string message = game.motivationChange < 0 ? "LOST CARGO " + std::to_string(game.motivationChange)
                : "DELIVERED " + std::to_string(game.lastDelivery) + "  +" + std::to_string(game.motivationChange);
            text(message, { 28, 113 }, game.motivationChange < 0 ? WarningColor : GoalColor);
        } else if (homeReady) {
            rectangle({ 16, 104, 360, 28 }, PanelColor);
            text("HOME IS OPEN - GO TO GREEN DOOR", { 28, 113 }, GoalColor);
        }

        if (game.outcome != Outcome::Playing || isPaused()) {
            const Vector2 center = { static_cast<float>(frame.window.width) / 2, bottom / 2 };
            const bool won = game.outcome == Outcome::Won;
            const std::string_view heading = won ? "HOME AT LAST" : (game.outcome == Outcome::Lost ? "NO MOTIVATION" : "PAUSED");
            const Color color = won ? GoalColor : (game.outcome == Outcome::Lost ? WarningColor : WhiteColor);
            rectangle({ center.x - 190, center.y - 65, 380, 130 }, PanelColor);
            rectangle({ center.x - 190, center.y - 65, 380, 130 }, color, false);
            text(heading, { center.x - static_cast<float>(heading.size()) * 6, center.y - 38 }, color, 3);
            if (game.outcome == Outcome::Lost) {
                text("REST AND TRY AGAIN", { center.x - 72, center.y - 4 }, WhiteColor);
            }
            text(game.outcome == Outcome::Playing ? "P TO CONTINUE" : "R TO RESTART", { center.x - 52, center.y + 25 }, WhiteColor);
        }
        return true;
    }
};
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#else
int main()
#endif
{
    Engine::ApplicationOptions options;
    options.window.title = "Cleanup";
    options.window.width = 1100;
    options.window.height = 720;
    options.window.minimumWidth = 640;
    options.window.minimumHeight = 420;
    Cleanup::CleanupApplication application;
    std::vector<Platform::Error> errors;
    const bool success = application.run(options, errors);
    bool showDialog = true;
    for (const auto& error : errors) {
        Platform::reportError(error, showDialog);
        showDialog = false;
    }
    return success ? 0 : 1;
}
