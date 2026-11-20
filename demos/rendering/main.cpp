#include "app/entry_point.h"
#include "app/application.h"
#include "core/camera.h"
#include "core/geometry.h"
#include "app/input.h"
#include "core/math2d.h"
#include "app/platform.h"
#include "graphics/renderer.h"
#include "core/transform.h"
#include "graphics/ui_view.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace
{
using namespace NeuexpEngine;

constexpr Color Ink = {0.06F, 0.10F, 0.14F};
constexpr Color White = {0.92F, 0.97F, 1};
constexpr Color Mint = {0.42F, 0.95F, 0.72F};
constexpr Color Gold = {1, 0.73F, 0.3F};
constexpr Vector2 ParkHalfSize = {510, 330};

constexpr int ShadowLayer = -2;
constexpr int ActorLayer = 0;

constexpr int HudHeaderHeight = 94;
constexpr int HudFooterHeight = 150;

constexpr std::array<Vector2, 5> TourPositions = {Vector2{-240, 70}, Vector2{180, 70},
    Vector2{-340, -140}, Vector2{0, 120}, Vector2{0, 120}};
constexpr std::array<Vector2, 6> Trees = {Vector2{-240, 50}, Vector2{-365, 140}, Vector2{-140, -80},
    Vector2{390, 190}, Vector2{280, 270}, Vector2{-260, 290}};
constexpr std::array<std::string_view, 5> TourTitles = {"1 / WALK BEHIND AND IN FRONT OF TREES",
    "2 / WALK UNDER THE GLASS CANOPY", "3 / WATCH THE WINDMILL BLADES",
    "4 / MOVE AND ZOOM THE WORLD", "5 / WATCH THE PARCEL WINDOW"};
constexpr std::array<std::string_view, 5> TourExplanations = {"FEET SET THE ORDER / LOWER FEET APPEAR IN FRONT",
    "WORLD SHOWS THROUGH GLASS / FRAME STAYS ABOVE IT",
    "LOCAL SHAPES ROTATE AND SCALE AROUND THE HUB",
    "MAP IS A SECOND VIEW / HUD STAYS FIXED ON SCREEN",
    "MOVING PARCELS ARE CUT AT THE WINDOW EDGES"};
constexpr std::array<std::string_view, 5> TourHints = {"Y TOGGLE SORTING / O REVERSE COMMANDS",
    "G TOGGLE GLASS / MOVE THROUGH THE BLUE AREA", "P PAUSE ROTATION / BLADES SHARE ONE PIVOT",
    "F FOLLOW CAMERA / WHEEL ZOOM / CLICK TO WALK", "C TOGGLE CLIPPING / L RESTART RENDERER"};

DrawOptions paint(Color color, int layer = 0, float order = 0, const Transform2& transform = Transform2{})
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
private:
    bool updateViews(const FrameInfo& frame, Platform::Error& error)
    {
        ready = false;
        if (frame.window.width <= 0 || frame.window.height <= 0) {
            return true;
        }
        const int width = frame.window.width;
        const int height = frame.window.height;
        CameraError cameraError;
        const Camera2 mapCamera = {Vector2{}, 1, Size2{1150, 800}, ViewportMode::Fit};
        if (!camera.createView(Viewport{0, HudHeaderHeight, width, height - HudHeaderHeight - HudFooterHeight - 14}, world, cameraError)) {
            error = Platform::Error{"Camera2::createView",
                "Could not calculate the world view. CameraError: " + std::to_string(static_cast<int>(cameraError)), std::nullopt};
            return false;
        }
        if (!mapCamera.createView(Viewport{width - 232, 110, 216, 150}, map, cameraError)) {
            error = Platform::Error{"Camera2::createView",
                "Could not calculate the map view. CameraError: " + std::to_string(static_cast<int>(cameraError)), std::nullopt};
            return false;
        }
        UiError uiError;
        if (!UiCanvas{}.createView(Viewport{0, 0, width, height}, ui, uiError)) {
            error = Platform::Error{"UiCanvas::createView",
                "Could not calculate the interface view. UiError: " + std::to_string(static_cast<int>(uiError)), std::nullopt};
            return false;
        }
        ready = true;
        return true;
    }

    bool onStart(Platform::Error& error) override
    {
        return initializeRenderer(error);
    }

    void handleToggles(const InputState& input)
    {
        if (input.key(Key::Escape).pressed) {
            requestExit();
        }
        if (input.key(Key::P).pressed) {
            setPaused(!isPaused());
        }
        if (input.key(Key::Y).pressed) {
            sorting = !sorting;
        }
        if (input.key(Key::O).pressed) {
            reverse = !reverse;
        }
        if (input.key(Key::G).pressed) {
            glass = !glass;
        }
        if (input.key(Key::C).pressed) {
            clipping = !clipping;
        }
        if (input.key(Key::F).pressed) {
            follow = !follow;
        }
    }

    void handleTour(const InputState& input)
    {
        if (input.key(Key::Tab).pressed) {
            tour = (tour + 1) % static_cast<int>(TourPositions.size());
            player = TourPositions[static_cast<std::size_t>(tour)];
            destination = player;
            walking = false;
            camera.position = Vector2{};
            camera.zoom = 1;
            follow = false;
        }
        if (input.key(Key::R).pressed) {
            player = TourPositions[static_cast<std::size_t>(tour)];
            walking = false;
            camera.position = Vector2{};
            camera.zoom = 1;
        }
    }

    bool restartRenderer(const InputState& input, Platform::Error& error)
    {
        if (!input.key(Key::L).pressed) {
            return true;
        }
        renderer.shutdown();
        if (!initializeRenderer(error)) {
            return false;
        }
        ++restarts;
        return true;
    }

    void handleCamera(const InputState& input)
    {
        if (!input.mouse().inside) {
            return;
        }
        const float zoom = camera.zoom + static_cast<float>(input.mouse().wheelY) * 0.1F;
        camera.zoom = std::clamp(zoom, 0.6F, 2.5F);
    }

    void handleMapClick(const InputState& input)
    {
        if (!ready || !input.button(MouseButton::Left).pressed) {
            return;
        }
        const Vector2 pixel = {static_cast<float>(input.mouse().x), static_cast<float>(input.mouse().y)};
        if (contains(map.viewport.bounds(), pixel)) {
            return;
        }
        walking = world.screenToWorld(pixel, destination);
        destination.x = std::clamp(destination.x, -ParkHalfSize.x, ParkHalfSize.x);
        destination.y = std::clamp(destination.y, -ParkHalfSize.y, ParkHalfSize.y);
    }

    bool onInput(const FrameInfo& frame, Platform::Error& error) override
    {
        const auto& input = Platform::getInput();
        handleToggles(input);
        handleTour(input);
        if (!restartRenderer(input, error)) {
            return false;
        }
        handleCamera(input);
        if (!updateViews(frame, error)) {
            return false;
        }
        handleMapClick(input);
        return true;
    }

    bool onUpdate(const FrameInfo& frame, Platform::Error&) override
    {
        constexpr float WalkSpeed = 180; // Мировые единицы в секунду.
        animation += frame.deltaTime;
        Vector2 direction;
        if (Platform::isKeyDown(Key::A)) {
            direction.x -= 1;
        }
        if (Platform::isKeyDown(Key::D)) {
            direction.x += 1;
        }
        if (Platform::isKeyDown(Key::W)) {
            direction.y -= 1;
        }
        if (Platform::isKeyDown(Key::S)) {
            direction.y += 1;
        }
        if (direction.x != 0 || direction.y != 0) {
            walking = false;
        } else if (walking) {
            direction = destination - player;
            if (length(direction) <= WalkSpeed * frame.deltaTime) {
                player = destination;
                walking = false;
                direction = Vector2{};
            }
        }
        if (direction.x != 0 || direction.y != 0) {
            facing = std::atan2(direction.y, direction.x);
            player += normalized(direction) * (WalkSpeed * frame.deltaTime);
        }
        player.x = std::clamp(player.x, -ParkHalfSize.x, ParkHalfSize.x);
        player.y = std::clamp(player.y, -ParkHalfSize.y, ParkHalfSize.y);
        if (follow) {
            camera.position = player;
        }
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

    void drawTree(Vector2 feet)
    {
        const float order = sorting ? feet.y : 0;
        renderer.drawCircle(Circle{feet + Vector2{5, 0}, 33}, paint(Color{0.09F, 0.18F, 0.15F, 0.4F}, ShadowLayer, order));
        renderer.drawRectangle(Rect{feet.x - 9, feet.y - 65, 18, 65}, paint(Color{0.45F, 0.28F, 0.18F}, ActorLayer, order));
        renderer.drawCircle(Circle{feet + Vector2{0, -78}, 47}, paint(Color{0.10F, 0.24F, 0.19F}, ActorLayer, order));
        renderer.drawCircle(Circle{feet + Vector2{-13, -89}, 33}, paint(Color{0.25F, 0.49F, 0.31F}, ActorLayer, order));
        renderer.drawCircle(Circle{feet + Vector2{14, -100}, 25}, paint(Color{0.35F, 0.61F, 0.37F}, ActorLayer, order));
    }

    void drawPlayer()
    {
        const float order = sorting ? player.y : 0;
        // Корпус остаётся вертикальным; при взгляде сбоку плечи видны уже.
        const float shoulderWidth = 20.0f + 12.0f * std::abs(std::sin(facing));
        renderer.drawCircle(Circle{player + Vector2{3, 1}, 17}, paint(Color{0.04F, 0.08F, 0.10F, 0.45F}, ShadowLayer, order));
        renderer.drawRectangle(Rect{player.x - shoulderWidth / 2, player.y - 32, shoulderWidth, 24}, paint(Gold, ActorLayer, order));
        renderer.drawCircle(Circle{player + Vector2{0, -41}, 13}, paint(White, ActorLayer, order));
        renderer.drawRectangle(Rect{player.x - 10, player.y - 21, 20, 16}, paint(Color{0.78F, 0.41F, 0.20F}, ActorLayer, order));
        renderer.drawCircle(Circle{player, 4}, paint(Mint, ActorLayer, order));
    }

    void drawGround()
    {
        renderer.drawRectangle(Rect{-560, -380, 1120, 760}, paint(Color{0.18F, 0.32F, 0.27F}, -20));
        constexpr int PathLayer = -15;
        renderer.drawRectangle(Rect{-540, 60, 1080, 80}, paint(Color{0.59F, 0.54F, 0.39F}, PathLayer));
        renderer.drawRectangle(Rect{-40, -350, 80, 700}, paint(Color{0.59F, 0.54F, 0.39F}, PathLayer));
    }

    void drawLandmarks()
    {
        constexpr int LandscapeLayer = -10;
        constexpr int LandscapeDetailLayer = -9;
        renderer.drawRectangle(Rect{85, -35, 255, 195}, paint(Color{0.29F, 0.39F, 0.39F}, LandscapeLayer));
        renderer.drawRectangle(Rect{105, -15, 215, 155}, paint(Color{0.39F, 0.48F, 0.47F}, LandscapeDetailLayer));
        renderer.drawCircle(Circle{Vector2{345, -220}, 85}, paint(Color{0.15F, 0.43F, 0.52F}, LandscapeLayer));
        renderer.drawCircle(Circle{Vector2{365, -235}, 57}, paint(Color{0.22F, 0.54F, 0.61F}, LandscapeDetailLayer));
        const Polygon garden = {Vector2{95, 210}, Vector2{185, 210}, Vector2{185, 240},
            Vector2{130, 240}, Vector2{130, 310}, Vector2{95, 310}};
        renderer.drawPolygon(garden, paint(Color{0.46F, 0.30F, 0.25F}, -8), ShapeFill::Filled);
        for (int flower = 0; flower < 4; ++flower) {
            renderer.drawCircle(Circle{Vector2{111, 225 + static_cast<float>(flower) * 23}, 6}, paint(Gold, -7));
        }
    }

    void drawWalkingTarget()
    {
        if (!walking) {
            return;
        }
        DrawOptions target = paint(Mint, -3);
        target.pointSize = 6;
        renderer.drawLine(player, destination, target);
        renderer.drawCircle(Circle{destination, 16}, target, ShapeFill::Outline);
        renderer.drawPoint(destination, target);
    }

    void drawTreesAndPlayer()
    {
        // Перестановка подачи не меняет результат при разных ключах Y.
        const int treeCount = static_cast<int>(Trees.size());
        for (int step = 0; step <= treeCount; ++step) {
            const int index = reverse ? treeCount - step : step;
            if (index == treeCount) {
                drawPlayer();
            } else {
                drawTree(Trees[static_cast<std::size_t>(index)]);
            }
        }
    }

    void drawMill()
    {
        constexpr float MillSortY = -70;
        renderer.drawRectangle(Rect{-385, -195, 90, 125}, paint(Color{0.79F, 0.70F, 0.51F}, ActorLayer, MillSortY));
        const Polygon roof = {Vector2{-402, -195}, Vector2{-340, -252}, Vector2{-278, -195}};
        renderer.drawPolygon(roof, paint(Color{0.62F, 0.31F, 0.22F}, ActorLayer, MillSortY), ShapeFill::Filled);
        const float stretch = 1 + 0.12F * std::sin(animation);
        const Transform2 rotor = {Vector2{-340, -175}, animation * 0.7F, Vector2{stretch, stretch}, Vector2{}};
        constexpr int RotorLayer = 1;
        for (int blade = 0; blade < 4; ++blade) {
            Transform2 transform = rotor;
            transform.rotation += static_cast<float>(blade) * Pi / 2;
            renderer.drawRectangle(Rect{8, -9, 69, 18}, paint(White, RotorLayer, 0, transform), ShapeFill::Filled);
        }
        renderer.drawCircle(Circle{rotor.position, 12}, paint(Gold, RotorLayer));
    }

    void drawGlass()
    {
        if (glass) {
            renderer.drawRectangle(Rect{95, -80, 235, 190}, paint(Color{0.3F, 0.8F, 0.95F, 0.38F}, 5));
        }
        constexpr int GlassFrameLayer = 6;
        renderer.drawRectangle(Rect{95, -80, 235, 190}, paint(Color{0.65F, 0.92F, 0.98F}, GlassFrameLayer), ShapeFill::Outline);
        renderer.drawRectangle(Rect{95, -80, 8, 190}, paint(White, GlassFrameLayer));
        renderer.drawRectangle(Rect{322, -80, 8, 190}, paint(White, GlassFrameLayer));
    }

    void drawTown(const CameraView& view)
    {
        renderer.setWorldView(view);
        drawGround();
        drawLandmarks();
        drawWalkingTarget();
        drawTreesAndPlayer();
        drawMill();
        drawGlass();
    }

    void drawHeader()
    {
        const float width = ui.size.width;
        renderer.drawRectangle(Rect{0, 0, width, HudHeaderHeight}, paint(Ink));
        renderer.drawDebugText("RENDERER TOWN", Vector2{22, 16}, Mint, 2);
        renderer.drawDebugText("WASD WALK / CLICK DESTINATION / WHEEL ZOOM", Vector2{22, 45}, White, 1);
        renderer.drawDebugText("TAB NEXT STOP / R RESET / ESC EXIT", Vector2{22, 66}, Gold, 1);
        renderer.drawDebugText("YOU ARE THE COURIER", Vector2{width - 264, 20}, Gold, 1);
        renderer.drawDebugText(isPaused() ? "PAUSED / P RESUME" : "P PAUSE / RESUME", Vector2{width - 264, 45}, White, 1);
    }

    void drawFooter()
    {
        const float width = ui.size.width;
        const float bottom = ui.size.height - HudFooterHeight;
        renderer.drawRectangle(Rect{0, bottom, width, HudFooterHeight}, paint(Ink));
        renderer.drawDebugText(TourTitles[static_cast<std::size_t>(tour)], Vector2{22, bottom + 16}, Mint, 1);
        renderer.drawDebugText(TourExplanations[static_cast<std::size_t>(tour)], Vector2{22, bottom + 40}, White, 1);
        renderer.drawDebugText(TourHints[static_cast<std::size_t>(tour)], Vector2{22, bottom + 62}, Gold, 1);
        renderer.drawDebugText(std::string("SORT ") + (sorting ? "ON" : "OFF") + " / GLASS "
                + (glass ? "ON" : "OFF") + " / CLIP " + (clipping ? "ON" : "OFF"),
            Vector2{22, bottom + 92}, White, 1);
        renderer.drawDebugText("RENDERER RESTARTS " + std::to_string(restarts), Vector2{22, bottom + 116}, Mint, 1);
    }

    void drawMapPanel()
    {
        const float width = ui.size.width;
        renderer.drawRectangle(map.viewport.bounds(), paint(Mint), ShapeFill::Outline);
        renderer.drawDebugText("LIVE MAP", Vector2{width - 224, 116}, White, 1);
    }

    void drawParcelWindow()
    {
        const float width = ui.size.width;
        const float bottom = ui.size.height - HudFooterHeight;
        // Лента остаётся в экранном пространстве при движении и масштабировании мира.
        const Rect slot = {width - 270, bottom + 38, 242, 62};
        renderer.drawDebugText("PARCEL WINDOW", Vector2{width - 270, bottom + 14}, Mint, 1);
        renderer.drawRectangle(slot, paint(Color{0.13F, 0.21F, 0.26F}));
        const Vector2 topLeft = ui.uiToScreen(slot.position);
        const Vector2 bottomRight = ui.uiToScreen(slot.position + Vector2{slot.size.width, slot.size.height});
        const Viewport clip = {static_cast<int>(std::ceil(topLeft.x)), static_cast<int>(std::ceil(topLeft.y)),
            static_cast<int>(bottomRight.x - topLeft.x), static_cast<int>(bottomRight.y - topLeft.y)};
        renderer.setClip(clipping ? std::optional<Viewport>{clip} : std::nullopt);
        for (int index = -1; index < 5; ++index) {
            const float x = slot.position.x + static_cast<float>(index) * 72.0f + std::fmod(animation * 32.0f, 72.0f);
            renderer.drawRectangle(Rect{x, slot.position.y + 12, 48, 38}, paint(Gold));
            renderer.drawRectangle(Rect{x + 21, slot.position.y + 12, 6, 38}, paint(Color{0.85F, 0.51F, 0.22F}));
        }
        renderer.setClip(std::nullopt);
        renderer.drawRectangle(slot, paint(White), ShapeFill::Outline);
        renderer.drawDebugText(clipping ? "C CLIP ON" : "C CLIP OFF", Vector2{width - 270, bottom + 116}, Gold, 1);
    }

    void drawHud()
    {
        renderer.setUiView(ui);
        drawHeader();
        drawFooter();
        drawMapPanel();
        drawParcelWindow();
    }

    bool onRender(const FrameInfo& frame, Platform::Error& error) override
    {
        if (!ready) {
            return true;
        }
        beginFrame(frame.window.width, frame.window.height, Ink);
        drawTown(world);
        drawTown(map);
        drawHud();
        return endFrame(error);
    }

    Renderer2D& renderer = getRenderer();
    Camera2 camera = {Vector2{}, 1, Size2{1000, 550}, ViewportMode::Expand};
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
};
}

int NeuexpEngine::applicationMain()
{
    RenderingApplication application;
    ApplicationOptions options;
    options.window.width = 1100;
    options.window.height = 800;
    options.window.title = "Renderer town";
    options.window.minimumWidth = 900;
    options.window.minimumHeight = 650;
    Platform::Error failure;
    if (!application.run(options, failure)) {
        Platform::reportError(failure);
        return 1;
    }
    return 0;
}
