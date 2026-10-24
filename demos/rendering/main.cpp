#include "application.h"
#include "renderer.h"
#include "../common/renderer_text.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace
{
using namespace Engine;

constexpr Color Ink = { 0.06F, 0.10F, 0.14F };
constexpr Color White = { 0.92F, 0.97F, 1 };
constexpr Color Mint = { 0.42F, 0.95F, 0.72F };
constexpr Color Gold = { 1, 0.73F, 0.3F };
constexpr Color Grass = { 0.18F, 0.32F, 0.27F };
constexpr float WalkSpeed = 180;
constexpr Vector2 ParkHalfSize = { 510, 330 };
constexpr std::array<Vector2, 5> TourPositions = {{ { -240, 70 }, { 180, 70 },
    { -340, -140 }, { 0, 120 }, { 0, 120 } }};
constexpr std::array<const char*, 5> TourTitles = {{ "1 / WALK BEHIND AND IN FRONT OF TREES",
    "2 / WALK UNDER THE GLASS CANOPY", "3 / WATCH THE WINDMILL BLADES",
    "4 / MOVE AND ZOOM THE WORLD", "5 / WATCH THE PARCEL WINDOW" }};
constexpr std::array<const char*, 5> TourHints = {{ "Y TOGGLE SORTING / O REVERSE COMMANDS",
    "G TOGGLE GLASS / MOVE THROUGH THE BLUE AREA", "P PAUSE ROTATION / BLADES SHARE ONE PIVOT",
    "F FOLLOW CAMERA / WHEEL ZOOM / CLICK TO WALK", "C TOGGLE CLIPPING / L RESTART RENDERER" }};
constexpr std::array<const char*, 5> TourExplanations = {{ "FEET SET THE ORDER / LOWER FEET APPEAR IN FRONT",
    "WORLD SHOWS THROUGH GLASS / FRAME STAYS ABOVE IT",
    "LOCAL SHAPES ROTATE AND SCALE AROUND THE HUB",
    "MAP IS A SECOND VIEW / HUD STAYS FIXED ON SCREEN",
    "MOVING PARCELS ARE CUT AT THE WINDOW EDGES" }};

DrawOptions paint(Color color, int layer = 0, float order = 0, const Transform2& transform = {})
{
    DrawOptions options;
    options.color = color;
    options.layer = layer;
    options.order = order;
    options.transform = transform.toAffine();
    return options;
}

class RenderingApplication final : public Application
{
    Renderer2D& renderer = getRenderer();
    Camera2 camera = { {}, 1, { 1000, 550 }, ViewportMode::Expand };
    CameraView world;
    CameraView map;
    UiView ui;
    Vector2 player = TourPositions[0];
    Vector2 destination = player;
    bool walking = false;
    bool sorting = true;
    bool reverse = false;
    bool glass = true;
    bool clipping = true;
    bool follow = false;
    bool ready = false;
    int tour = 0;
    int restarts = 0;
    float animation = 0;
    float facing = Pi / 2;
    RenderError drawError = RenderError::None;

    bool rectangle(Rect bounds, Color color, int layer = 0, float order = 0)
    {
        return renderer.drawRectangle(bounds, ShapeFill::Filled, paint(color, layer, order), drawError);
    }

    bool circle(Vector2 center, float radius, Color color, int layer = 0, float order = 0)
    {
        return renderer.drawCircle({ center, radius }, ShapeFill::Filled, paint(color, layer, order), drawError);
    }

    bool text(const std::string& message, Vector2 position, Color color = White, float size = 2)
    {
        return Demo::text(renderer, message, position, color, drawError, size);
    }

    bool rendererResult(bool success, Platform::Error& error)
    {
        if (!success) {
            error = { "Renderer town", "Rendering failed (RenderError="
                + std::to_string(static_cast<int>(drawError)) + ").", {} };
        }
        return success;
    }

    bool updateViews(const FrameInfo& frame, Platform::Error& error)
    {
        ready = false;
        if (frame.window.width <= 0 || frame.window.height <= 0) {
            return true;
        }
        const int width = frame.window.width;
        const int height = frame.window.height;
        CameraError cameraError;
        UiError uiError;
        Camera2 mapCamera = { {}, 1, { 1150, 800 }, ViewportMode::Fit };
        if (!camera.createView({ 0, 94, width, height - 258 }, world, cameraError)
            || !mapCamera.createView({ width - 232, 110, 216, 150 }, map, cameraError)
            || !UiCanvas{}.createView({ 0, 0, width, height }, ui, uiError)) {
            error = { "Renderer town", "Could not calculate world/UI views.", {} };
            return false;
        }
        ready = true;
        return true;
    }

    bool onStart(Platform::Error& error) override
    {
        return rendererResult(renderer.initialize(drawError), error);
    }

    bool onInput(const FrameInfo& frame, Platform::Error& error) override
    {
        const auto& input = Platform::getInput();
        if (input.key(Key::Escape).pressed) { requestExit(); }
        if (input.key(Key::P).pressed) { setPaused(!isPaused()); }
        if (input.key(Key::Y).pressed) { sorting = !sorting; }
        if (input.key(Key::O).pressed) { reverse = !reverse; }
        if (input.key(Key::G).pressed) { glass = !glass; }
        if (input.key(Key::C).pressed) { clipping = !clipping; }
        if (input.key(Key::F).pressed) { follow = !follow; }
        if (input.key(Key::Tab).pressed) {
            tour = (tour + 1) % static_cast<int>(TourPositions.size());
            player = TourPositions[static_cast<std::size_t>(tour)];
            destination = player;
            walking = false;
            camera.position = {};
            camera.zoom = 1;
            follow = false;
        }
        if (input.key(Key::R).pressed) {
            player = TourPositions[static_cast<std::size_t>(tour)];
            walking = false;
            camera.position = {};
            camera.zoom = 1;
        }
        if (input.key(Key::L).pressed) {
            renderer.shutdown();
            if (!rendererResult(renderer.initialize(drawError), error)) {
                return false;
            }
            ++restarts;
        }
        if (input.mouse().inside) {
            camera.zoom = std::clamp(camera.zoom + static_cast<float>(input.mouse().wheelY) * 0.1F, 0.6F, 2.5F);
        }
        if (!updateViews(frame, error)) {
            return false;
        }
        if (ready && input.button(MouseButton::Left).pressed) {
            const Vector2 pixel = { static_cast<float>(input.mouse().x), static_cast<float>(input.mouse().y) };
            if (!contains(map.viewport.bounds(), pixel)) {
                walking = world.screenToWorld(pixel, destination);
                destination.x = std::clamp(destination.x, -ParkHalfSize.x, ParkHalfSize.x);
                destination.y = std::clamp(destination.y, -ParkHalfSize.y, ParkHalfSize.y);
            }
        }
        return true;
    }

    bool onUpdate(const FrameInfo& frame, Platform::Error&) override
    {
        animation += frame.deltaTime;
        Vector2 direction;
        if (Platform::isKeyDown(Key::A)) { direction.x -= 1; }
        if (Platform::isKeyDown(Key::D)) { direction.x += 1; }
        if (Platform::isKeyDown(Key::W)) { direction.y -= 1; }
        if (Platform::isKeyDown(Key::S)) { direction.y += 1; }
        if (direction.x != 0 || direction.y != 0) {
            walking = false;
        } else if (walking) {
            direction = destination - player;
            if (length(direction) <= WalkSpeed * frame.deltaTime) {
                player = destination;
                walking = false;
                direction = {};
            }
        }
        if (direction.x != 0 || direction.y != 0) {
            facing = std::atan2(direction.y, direction.x);
            player += normalized(direction) * (WalkSpeed * frame.deltaTime);
        }
        player.x = std::clamp(player.x, -ParkHalfSize.x, ParkHalfSize.x);
        player.y = std::clamp(player.y, -ParkHalfSize.y, ParkHalfSize.y);
        if (follow) { camera.position = player; }
        return true;
    }

    bool onPrepareFrame(const FrameInfo& frame, Platform::Error& error) override
    {
        return updateViews(frame, error)
            && Platform::setWindowTitle("Renderer town | tour=" + std::to_string(tour + 1)
                + " sort=" + (sorting ? "on" : "off") + " commands=" + (reverse ? "reverse" : "forward")
                + " glass=" + (glass ? "on" : "off") + " clip=" + (clipping ? "on" : "off")
                + " restarts=" + std::to_string(restarts), error);
    }

    bool drawTree(Vector2 feet)
    {
        const float order = sorting ? feet.y : 0;
        return circle(feet + Vector2{ 5, 0 }, 33, { 0.09F, 0.18F, 0.15F, 0.4F }, -2, order)
            && rectangle({ feet.x - 9, feet.y - 65, 18, 65 }, { 0.45F, 0.28F, 0.18F }, 0, order)
            && circle(feet + Vector2{ 0, -78 }, 47, { 0.10F, 0.24F, 0.19F }, 0, order)
            && circle(feet + Vector2{ -13, -89 }, 33, { 0.25F, 0.49F, 0.31F }, 0, order)
            && circle(feet + Vector2{ 14, -100 }, 25, { 0.35F, 0.61F, 0.37F }, 0, order);
    }

    bool drawPlayer()
    {
        const float order = sorting ? player.y : 0;
        // Корпус остаётся вертикальным; при взгляде сбоку плечи видны уже.
        const float shoulderWidth = 20 + 12 * std::abs(std::sin(facing));
        return circle(player + Vector2{ 3, 1 }, 17, { 0.04F, 0.08F, 0.10F, 0.45F }, -2, order)
            && rectangle({ player.x - shoulderWidth / 2, player.y - 32, shoulderWidth, 24 }, Gold, 0, order)
            && circle(player + Vector2{ 0, -41 }, 13, White, 0, order)
            && rectangle({ player.x - 10, player.y - 21, 20, 16 }, { 0.78F, 0.41F, 0.20F }, 0, order)
            && circle(player, 4, Mint, 0, order);
    }

    bool drawTown(const CameraView& view)
    {
        if (!renderer.beginWorld(view, drawError)
            || !rectangle({ -560, -380, 1120, 760 }, Grass, -20)
            || !rectangle({ -540, 60, 1080, 80 }, { 0.59F, 0.54F, 0.39F }, -15)
            || !rectangle({ -40, -350, 80, 700 }, { 0.59F, 0.54F, 0.39F }, -15)
            || !rectangle({ 85, -35, 255, 195 }, { 0.29F, 0.39F, 0.39F }, -10)
            || !rectangle({ 105, -15, 215, 155 }, { 0.39F, 0.48F, 0.47F }, -9)
            || !circle({ 345, -220 }, 85, { 0.15F, 0.43F, 0.52F }, -10)
            || !circle({ 365, -235 }, 57, { 0.22F, 0.54F, 0.61F }, -9)) {
            return false;
        }
        const std::vector<Vector2> garden = { { 95, 210 }, { 185, 210 }, { 185, 240 },
            { 130, 240 }, { 130, 310 }, { 95, 310 } };
        if (!renderer.drawPolygon(garden, ShapeFill::Filled, paint({ 0.46F, 0.30F, 0.25F }, -8), drawError)) {
            return false;
        }
        for (int flower = 0; flower < 4; ++flower) {
            if (!circle({ 111, 225 + static_cast<float>(flower) * 23 }, 6, Gold, -7)) {
                return false;
            }
        }
        if (walking) {
            DrawOptions target = paint(Mint, -3);
            target.pointSize = 6;
            if (!renderer.drawLine(player, destination, target, drawError)
                || !renderer.drawCircle({ destination, 16 }, ShapeFill::Outline, target, drawError)
                || !renderer.drawPoint(destination, target, drawError)) {
                return false;
            }
        }
        constexpr std::array<Vector2, 6> Trees = {{ { -240, 50 }, { -365, 140 }, { -140, -80 },
            { 390, 190 }, { 280, 270 }, { -260, 290 } }};
        // Перестановка подачи не меняет результат при разных ключах Y.
        for (int step = 0; step < 7; ++step) {
            const int index = reverse ? 6 - step : step;
            if (!(index == 6 ? drawPlayer() : drawTree(Trees[static_cast<std::size_t>(index)]))) {
                return false;
            }
        }
        if (!rectangle({ -385, -195, 90, 125 }, { 0.79F, 0.70F, 0.51F }, 0, -70)) {
            return false;
        }
        const std::vector<Vector2> roof = { { -402, -195 }, { -340, -252 }, { -278, -195 } };
        if (!renderer.drawPolygon(roof, ShapeFill::Filled, paint({ 0.62F, 0.31F, 0.22F }, 0, -70), drawError)) {
            return false;
        }
        const float stretch = 1 + 0.12F * std::sin(animation);
        const Transform2 rotor = { { -340, -175 }, animation * 0.7F, { stretch, stretch }, {} };
        for (int blade = 0; blade < 4; ++blade) {
            Transform2 transform = rotor;
            transform.rotation += static_cast<float>(blade) * Pi / 2;
            if (!renderer.drawRectangle({ 8, -9, 69, 18 }, ShapeFill::Filled,
                paint(White, 1, 0, transform), drawError)) {
                return false;
            }
        }
        if (!circle(rotor.position, 12, Gold, 1)) { return false; }
        if (glass && !rectangle({ 95, -80, 235, 190 }, { 0.3F, 0.8F, 0.95F, 0.38F }, 5)) {
            return false;
        }
        if (!renderer.drawRectangle({ 95, -80, 235, 190 }, ShapeFill::Outline,
                paint({ 0.65F, 0.92F, 0.98F }, 6), drawError)
            || !rectangle({ 95, -80, 8, 190 }, White, 6)
            || !rectangle({ 322, -80, 8, 190 }, White, 6)) {
            return false;
        }
        return true;
    }

    bool drawHud()
    {
        const float width = ui.size.width;
        const float bottom = ui.size.height - 150;
        if (!renderer.beginUi(ui, drawError)
            || !rectangle({ 0, 0, width, 94 }, Ink)
            || !text("RENDERER TOWN", { 22, 16 }, Mint, 3)
            || !text("WASD WALK / CLICK DESTINATION / WHEEL ZOOM", { 22, 45 })
            || !text("TAB NEXT STOP / R RESET / ESC EXIT", { 22, 66 }, Gold)
            || !text("YOU ARE THE COURIER", { width - 264, 20 }, Gold)
            || !text(isPaused() ? "PAUSED / P RESUME" : "P PAUSE / RESUME", { width - 264, 45 })
            || !rectangle({ 0, bottom, width, 150 }, Ink)
            || !text(TourTitles[static_cast<std::size_t>(tour)], { 22, bottom + 16 }, Mint)
            || !text(TourExplanations[static_cast<std::size_t>(tour)], { 22, bottom + 40 }, White, 1.5F)
            || !text(TourHints[static_cast<std::size_t>(tour)], { 22, bottom + 62 }, Gold, 1.5F)
            || !text(std::string("SORT ") + (sorting ? "ON" : "OFF") + " / GLASS " + (glass ? "ON" : "OFF")
                + " / CLIP " + (clipping ? "ON" : "OFF"), { 22, bottom + 92 }, White, 1.5F)
            || !text("RENDERER RESTARTS " + std::to_string(restarts), { 22, bottom + 116 }, Mint, 1.5F)) {
            return false;
        }
        if (!renderer.drawRectangle(map.viewport.bounds(), ShapeFill::Outline, paint(Mint), drawError)
            || !text("LIVE MAP", { width - 224, 116 }, White, 1.5F)) {
            return false;
        }
        // Лента остаётся в экранном пространстве при движении и масштабировании мира.
        const Rect slot = { width - 270, bottom + 38, 242, 62 };
        if (!text("PARCEL WINDOW", { width - 270, bottom + 14 }, Mint)
            || !rectangle(slot, { 0.13F, 0.21F, 0.26F })) {
            return false;
        }
        const Vector2 topLeft = ui.uiToScreen(slot.position);
        const Vector2 bottomRight = ui.uiToScreen(slot.position + Vector2{ slot.size.width, slot.size.height });
        const Viewport clip = { static_cast<int>(std::ceil(topLeft.x)), static_cast<int>(std::ceil(topLeft.y)),
            static_cast<int>(bottomRight.x - topLeft.x), static_cast<int>(bottomRight.y - topLeft.y) };
        if (!renderer.setClip(clipping ? std::optional<Viewport>{ clip } : std::nullopt, drawError)) {
            return false;
        }
        for (int index = -1; index < 5; ++index) {
            const float x = slot.position.x + static_cast<float>(index) * 72 + std::fmod(animation * 32, 72.0F);
            if (!rectangle({ x, slot.position.y + 12, 48, 38 }, Gold)
                || !rectangle({ x + 21, slot.position.y + 12, 6, 38 }, { 0.85F, 0.51F, 0.22F })) {
                return false;
            }
        }
        return renderer.setClip(std::nullopt, drawError)
            && renderer.drawRectangle(slot, ShapeFill::Outline, paint(White), drawError)
            && text(clipping ? "C CLIP ON" : "C CLIP OFF", { width - 270, bottom + 116 }, Gold, 1.5F);
    }

    bool onRender(const FrameInfo& frame, Platform::Error& error) override
    {
        if (!ready) { return true; }
        if (!renderer.beginFrame(frame.window.width, frame.window.height, Ink, drawError)) {
            return rendererResult(false, error);
        }
        bool success = drawTown(world) && drawTown(map) && drawHud();
        RenderError endError;
        const bool ended = renderer.endFrame(endError);
        if (success && !ended) {
            drawError = endError;
            success = false;
        }
        return rendererResult(success, error);
    }
};
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#else
int main()
#endif
{
    RenderingApplication application;
    ApplicationOptions options;
    options.window.width = 1100;
    options.window.height = 800;
    options.window.title = "Renderer town";
    options.window.minimumWidth = 900;
    options.window.minimumHeight = 650;
    std::vector<Platform::Error> errors;
    if (!application.run(options, errors)) {
        for (const auto& error : errors) { Platform::reportError(error); }
        return 1;
    }
    return 0;
}
