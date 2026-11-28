#include "app/application.h"
#include "app/entry_point.h"
#include "app/input.h"
#include "app/platform.h"
#include "core/camera.h"
#include "core/math2d.h"
#include "graphics/renderer.h"
#include "world.h"

#include <iomanip>
#include <sstream>
#include <string>

namespace NE = NeuexpEngine;

namespace
{
constexpr char WindowTitle[] = "Physics world";
constexpr int WindowWidth = 1100;
constexpr int WindowHeight = 700;
constexpr float FixedStep = 1.0F / 60;
constexpr int MaximumCatchUpSteps = 5;

const NE::Color Background{0.03F, 0.04F, 0.07F, 1};
const NE::Color TitleColor{0.55F, 0.85F, 0.95F, 1};
const NE::Color TextColor{0.88F, 0.9F, 0.94F, 1};
const NE::Color HintColor{0.95F, 0.8F, 0.4F, 1};

std::string formatNumber(float value, int precision)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

// Демонстрация физического мира: падающие и отскакивающие тела, вращающиеся ящики,
// движущаяся площадка, сенсоры, силы и импульсы. Симуляция идёт постоянным шагом,
// поэтому поведение не зависит от частоты кадров, а задержка не догоняется бесконечно.
class PhysicsApplication final : public NE::Application
{
private:
    bool onStart(Platform::Error& error) override
    {
        if (!initializeRenderer(error)) {
            return false;
        }
        NE::ResourceError resourceError;
        if (!demo.loadAssets(getRenderer(), resourceError)) {
            error = Platform::Error{"Renderer2D::createTexture", resourceError.message, std::nullopt};
            return false;
        }
        demo.recreate();
        camera.referenceSize = NE::Size2{PhysicsDemo::WorldWidth, PhysicsDemo::WorldHeight};
        camera.position = NE::Vector2{PhysicsDemo::WorldWidth / 2, PhysicsDemo::WorldHeight / 2};
        camera.mode = NE::ViewportMode::Fit;
        return true;
    }

    bool onInput(const NE::FrameInfo& frame, Platform::Error&) override
    {
        const NE::InputState& input = Platform::getInput();
        if (Platform::isKeyDown(Platform::Key::Escape)) {
            requestExit();
        }
        if (Platform::isKeyPressed(Platform::Key::P)) {
            setPaused(!isPaused());
        }
        if (Platform::isKeyPressed(Platform::Key::R)) {
            demo.recreate();
        }
        if (Platform::isKeyPressed(Platform::Key::F)) {
            demo.toggleWind();
        }
        if (Platform::isKeyPressed(Platform::Key::Space)) {
            demo.spawnBall();
        }

        NE::CameraView view;
        NE::CameraError cameraError;
        // Без корректного вида мышь не переводится в мир; ввод клавиш уже обработан.
        if (!camera.createView(NE::Viewport{0, 0, frame.window.width, frame.window.height}, view, cameraError)) {
            demo.setAttract(false);
            return true;
        }
        NE::Vector2 worldPoint;
        if (input.mouse().inside
            && view.screenToWorld(NE::Vector2{static_cast<float>(input.mouse().x),
                                      static_cast<float>(input.mouse().y)},
                worldPoint)) {
            demo.setCaret(worldPoint);
            if (input.button(NE::MouseButton::Left).pressed) {
                demo.impulseAt(worldPoint);
            }
        }
        demo.setAttract(input.mouse().inside && input.button(NE::MouseButton::Right).down);
        return true;
    }

    bool onUpdate(const NE::FrameInfo& frame, Platform::Error&) override
    {
        // Постоянный шаг даёт один вызов обновления на шаг симуляции.
        demo.step(frame.deltaTime);
        return true;
    }

    bool onRender(const NE::FrameInfo& frame, Platform::Error& error) override
    {
        NE::CameraView view;
        NE::CameraError cameraError;
        if (!camera.createView(NE::Viewport{0, 0, frame.window.width, frame.window.height}, view, cameraError)) {
            error = Platform::Error{"Camera2::createView", "Could not calculate the world view. CameraError: "
                + std::to_string(static_cast<int>(cameraError)), std::nullopt};
            return false;
        }
        NE::Renderer2D& drawing = getRenderer();
        beginFrame(frame.window.width, frame.window.height, Background);
        drawing.setWorldView(view);
        demo.draw(drawing);
        drawHud(drawing, frame);
        return endFrame(error);
    }

    void drawHud(NE::Renderer2D& drawing, const NE::FrameInfo& frame) const
    {
        const PhysicsDemo::Statistics& stats = demo.statistics();
        drawing.drawDebugText("PHYSICS WORLD: FALLING, BOUNCING, ROTATING BODIES AND SENSORS",
            NE::Vector2{18, 12}, TitleColor, 2);
        drawing.drawDebugText("BODIES " + std::to_string(demo.bodyCount()) + "   CONTACTS "
                + std::to_string(stats.contacts) + "   IMPACTS " + std::to_string(stats.impacts)
                + "   LAST IMPULSE " + formatNumber(stats.lastImpactImpulse, 0),
            NE::Vector2{18, 40}, TextColor, 1);
        drawing.drawDebugText("GATE INSIDE " + std::to_string(stats.gateInside) + "   BEGIN "
                + std::to_string(stats.gateBegins) + "   END " + std::to_string(stats.gateEnds)
                + "   PIT REMOVED " + std::to_string(stats.pitRemoved),
            NE::Vector2{18, 58}, TextColor, 1);
        const std::string step = "FIXED STEP " + formatNumber(FixedStep, 4) + " S   UPDATES "
            + std::to_string(frame.updates) + " OF " + std::to_string(MaximumCatchUpSteps)
            + "   FPS " + formatNumber(static_cast<float>(frame.framesPerSecond), 0)
            + "   WIND " + (demo.isWindActive() ? "ON" : "OFF") + (isPaused() ? "   PAUSED" : "");
        drawing.drawDebugText(step, NE::Vector2{18, 76}, TextColor, 1);
        drawing.drawDebugText("LMB IMPULSE AT POINT   RMB ATTRACT   F WIND   SPACE BALL   R RESTART   P PAUSE   ESC EXIT",
            NE::Vector2{18, 94}, HintColor, 1);
    }

    NE::Camera2 camera;
    PhysicsDemo::World demo;
};
}

int NeuexpEngine::applicationMain()
{
    NE::ApplicationOptions options;
    options.window.width = WindowWidth;
    options.window.height = WindowHeight;
    options.window.minimumWidth = 550;
    options.window.minimumHeight = 350;
    options.window.resizable = true;
    options.window.title = WindowTitle;
    options.fixedStepTime = FixedStep;
    options.maxCatchUpSteps = MaximumCatchUpSteps;
    PhysicsApplication application;
    Platform::Error failure;
    const bool success = application.run(options, failure);
    if (!success) {
        Platform::reportError(failure);
    }
    return success ? 0 : 1;
}
