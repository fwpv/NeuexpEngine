#include "application.h"
#include "camera.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <sstream>
#include <vector>

namespace
{
using Engine::Affine2;
using Engine::Camera2;
using Engine::CameraView;
using Engine::Circle;
using Engine::Color;
using Engine::Rect;
using Engine::Vector2;

constexpr Rect WorldBounds = { 0, 0, 3000, 2100 };
constexpr float PlayerRadius = 18;
constexpr float TrashRadius = 8;
constexpr float PlayerSpeed = 260;
constexpr float GridStep = 100;
constexpr float GoldenAngle = 2.39996323F;
constexpr int TrashCount = 60;
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
constexpr std::array<Color, 3> TrashColors = {{
    { 0.94F, 0.84F, 0.43F }, { 0.75F, 0.55F, 0.85F }, { 0.67F, 0.78F, 0.81F }
}};

struct Puddle
{
    Circle shape;
    bool sticky;
};

constexpr std::array<Puddle, 6> Puddles = {{
    { { { 1280, 1050 }, 80 }, true }, { { { 1740, 1050 }, 85 }, false },
    { { { 520, 480 }, 95 }, true }, { { { 2420, 490 }, 100 }, false },
    { { { 2440, 1680 }, 90 }, true }, { { { 560, 1660 }, 95 }, false }
}};

enum class Coat
{
    Dry,
    Sticky,
    Clean
};

enum class TrashState
{
    Ground,
    Carried,
    Washed
};

struct Trash
{
    Vector2 position;
    Vector2 attachment;
    TrashState state = TrashState::Ground;
};

struct Game
{
    Vector2 player = WorldBounds.center();
    float heading = 0;
    Coat coat = Coat::Dry;
    std::array<Trash, TrashCount> trash;
    int carried = 0;
    int washed = 0;
    std::optional<Vector2> target;

    Game()
    {
        // Первые предметы расположены рядом с начальной оранжевой лужей.
        trash[0].position = { 1410, 1050 };
        trash[1].position = { 1450, 1100 };
        trash[2].position = { 1370, 1150 };
        for (int index = 3; index < TrashCount; ++index) {
            Vector2 point = {
                160.0F + static_cast<float>((index * 463) % 2680),
                150.0F + static_cast<float>((index * 347) % 1800)
            };
            for (const Puddle& puddle : Puddles) {
                if (Engine::intersects(puddle.shape, Circle{ point, TrashRadius + 20 })) {
                    point.x += 190;
                }
            }
            trash[static_cast<std::size_t>(index)].position = Engine::closestPoint({ 100, 100, 2800, 1900 }, point);
        }
    }

    void update(Vector2 direction, float deltaTime)
    {
        if (direction != Vector2{}) {
            target.reset();
            direction = Engine::normalized(direction);
        } else if (target) {
            const Vector2 offset = *target - player;
            const float remaining = Engine::length(offset);
            if (remaining <= PlayerSpeed * deltaTime) {
                player = *target;
                target.reset();
            } else {
                direction = Engine::normalized(offset);
            }
        }
        if (direction != Vector2{}) {
            player += direction * PlayerSpeed * deltaTime;
            heading = std::atan2(direction.y, direction.x);
        }
        player = Engine::closestPoint({ PlayerRadius, PlayerRadius,
            WorldBounds.size.width - 2 * PlayerRadius, WorldBounds.size.height - 2 * PlayerRadius }, player);

        bool inOrange = false;
        bool inBlue = false;
        for (const Puddle& puddle : Puddles) {
            if (Engine::intersects(puddle.shape, Circle{ player, PlayerRadius })) {
                inOrange = inOrange || puddle.sticky;
                inBlue = inBlue || !puddle.sticky;
            }
        }
        if (inBlue) {
            coat = Coat::Clean;
            for (Trash& item : trash) {
                if (item.state == TrashState::Carried) {
                    item.state = TrashState::Washed;
                }
            }
            washed += carried;
            carried = 0;
        } else if (inOrange) {
            coat = Coat::Sticky;
        }

        if (coat == Coat::Sticky) {
            const float pickupRadius = PlayerRadius + std::min(static_cast<float>(carried), 24.0F);
            for (Trash& item : trash) {
                if (item.state == TrashState::Ground
                    && Engine::intersects(Circle{ player, pickupRadius }, Circle{ item.position, TrashRadius })) {
                    item.state = TrashState::Carried;
                    item.attachment = Engine::rotated({ PlayerRadius + 9 + 4 * std::sqrt(static_cast<float>(carried)), 0 },
                        static_cast<float>(carried) * GoldenAngle);
                    ++carried;
                }
            }
        }
    }
};

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

void drawWorld(const Game& game)
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
    for (const Puddle& puddle : Puddles) {
        circle(puddle.shape, puddle.sticky ? OrangePoolColor : BluePoolColor);
        circle({ puddle.shape.center, puddle.shape.radius - 12 }, puddle.sticky ? StickyPlayerColor : CleanPlayerColor, false);
    }
    if (game.target) {
        circle({ *game.target, 14 }, WhiteColor, false);
        circle({ *game.target, 4 }, WhiteColor);
    }

    const Color bodyColor = playerColor(game.coat);
    circle({ game.player, PlayerRadius + 3 }, WhiteColor);
    circle({ game.player, PlayerRadius }, bodyColor);
    const Affine2 playerTransform = Engine::Transform2{ game.player, game.heading, { 1, 1 }, {} }.toAffine();
    circle({ playerTransform.transformPoint({ 8, -6 }), 3 }, PanelColor);
    circle({ playerTransform.transformPoint({ 8, 6 }), 3 }, PanelColor);
    for (std::size_t index = 0; index < game.trash.size(); ++index) {
        const Trash& item = game.trash[index];
        if (item.state == TrashState::Washed) {
            continue;
        }
        const Vector2 position = item.state == TrashState::Carried
            ? playerTransform.transformPoint(item.attachment) : item.position;
        const Color color = TrashColors[index % TrashColors.size()];
        if (index % 3 == 0) {
            circle({ position, TrashRadius }, color);
        } else {
            const float width = index % 3 == 1 ? TrashRadius * 2 : TrashRadius;
            rectangle({ position.x - width / 2, position.y - TrashRadius, width, TrashRadius * 2 }, color);
        }
    }
}

// Цифры состоят из прямоугольников; значение показывается без шрифтовых ресурсов.
void number(int value, Vector2 position, Color color)
{
    constexpr std::array<unsigned int, 10> Segments = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
    constexpr std::array<Rect, 7> Bars = {{
        { 3, 0, 12, 3 }, { 15, 3, 3, 12 }, { 15, 18, 3, 12 }, { 3, 30, 12, 3 },
        { 0, 18, 3, 12 }, { 0, 3, 3, 12 }, { 3, 15, 12, 3 }
    }};
    const int digits[] = { value / 10, value % 10 };
    for (int digit : digits) {
        const unsigned int mask = Segments[static_cast<std::size_t>(digit)];
        for (std::size_t index = 0; index < Bars.size(); ++index) {
            if ((mask & (1U << index)) != 0) {
                rectangle({ position + Bars[index].position, Bars[index].size }, color);
            }
        }
        position.x += 24;
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
            camera.zoom = 1;
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
        if (viewsReady) {
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
        game.update(direction, frame.deltaTime);
        titleDirty = titleDirty || previousCoat != game.coat || previousCarried != game.carried || previousWashed != game.washed;
        return true;
    }

    bool onPrepareFrame(const Engine::FrameInfo& frame, Platform::Error& error) override
    {
        if (!updateViews(frame, error)) {
            return false;
        }
        if (titleDirty || frame.fpsUpdated) {
            const char* coat = game.coat == Coat::Sticky ? "sticky" : (game.coat == Coat::Clean ? "clean" : "dry");
            std::ostringstream title;
            title << "Cleanup | coat=" << coat << " carry=" << game.carried << " washed=" << game.washed << '/' << TrashCount
                << (game.washed == TrashCount ? " | ALL CLEAN!" : " | orange: glue / blue: wash")
                << " | WASD/arrows, click: walk, wheel: zoom, F: "
                << (camera.mode == Engine::ViewportMode::Fit ? "fit" : "expand") << ", R: reset";
            if (!Platform::setWindowTitle(title.str(), error)) {
                return false;
            }
            titleDirty = false;
        }
        return true;
    }

    void renderView(const CameraView& view, int windowHeight, bool map)
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
        drawWorld(game);
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
            circle({ game.player, 3 * unitsPerPixel }, playerColor(game.coat));
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
        renderView(mainView, frame.window.height, false);
        screenCoordinates(frame);
        rectangle(mapPanel, PanelColor);
        rectangle(mapPanel, BorderColor, false);
        renderView(mapView, frame.window.height, true);
        screenCoordinates(frame);
        // Слева — налипший мусор, справа — смытый; полоска показывает долю очищенного мира.
        rectangle({ 16, 16, 228, 61 }, PanelColor);
        circle({ { 40, 42 }, 13 }, playerColor(game.coat));
        number(game.carried, { 64, 24 }, StickyPlayerColor);
        circle({ { 146, 42 }, 13 }, BluePoolColor);
        number(game.washed, { 169, 24 }, CleanPlayerColor);
        rectangle({ 16, 81, 228, 7 }, GridColor);
        rectangle({ 16, 81, 228 * static_cast<float>(game.washed) / TrashCount, 7 }, GoalColor);
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
    CleanupApplication application;
    std::vector<Platform::Error> errors;
    const bool success = application.run(options, errors);
    bool showDialog = true;
    for (const auto& error : errors) {
        Platform::reportError(error, showDialog);
        showDialog = false;
    }
    return success ? 0 : 1;
}
