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
constexpr char WindowTitle[] = "Physics checks";
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

// Демонстрация — фабрика проверок: три страницы показывают взаимодействия тел,
// события в областях и достигнутые стандарты индустрии. Каждая зона подписывает,
// что именно она проверяет и что происходит сейчас.
class PhysicsApplication final : public NE::Application
{
private:
    bool onStart(Platform::Error& error) override
    {
        if (!initializeRenderer(error)) {
            return false;
        }
        board.build();
        camera.referenceSize = NE::Size2{PhysicsDemo::WorldWidth, PhysicsDemo::WorldHeight};
        camera.position = NE::Vector2{PhysicsDemo::WorldWidth / 2, PhysicsDemo::WorldHeight / 2};
        camera.mode = NE::ViewportMode::Fit;
        return true;
    }

    bool onInput(const NE::FrameInfo&, Platform::Error&) override
    {
        if (Platform::isKeyDown(Platform::Key::Escape)) {
            requestExit();
        }
        if (Platform::isKeyPressed(Platform::Key::Tab)) {
            board.nextPage();
        }
        if (Platform::isKeyPressed(Platform::Key::P)) {
            setPaused(!isPaused());
        }
        if (Platform::isKeyPressed(Platform::Key::R)) {
            board.restart();
        }
        return true;
    }

    bool onUpdate(const NE::FrameInfo& frame, Platform::Error&) override
    {
        // Постоянный шаг даёт один вызов обновления на шаг симуляции.
        board.update(frame.deltaTime);
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
        board.draw(drawing);
        drawHud(drawing, frame);
        return endFrame(error);
    }

    void drawHud(NE::Renderer2D& drawing, const NE::FrameInfo& frame) const
    {
        drawing.drawDebugText("PHYSICS CHECKS: INTERACTIONS, EVENTS AND INDUSTRY-STANDARD GUARANTEES",
            NE::Vector2{18, 12}, TitleColor, 2);
        const std::string page = "PAGE " + std::to_string(board.pageIndex() + 1) + " OF "
            + std::to_string(board.pageCount()) + ": " + board.pageName()
            + "   STEP " + formatNumber(FixedStep, 4) + " S   UPDATES "
            + std::to_string(frame.updates) + " OF " + std::to_string(MaximumCatchUpSteps)
            + "   FPS " + formatNumber(static_cast<float>(frame.framesPerSecond), 0)
            + (isPaused() ? "   PAUSED" : "");
        drawing.drawDebugText(page, NE::Vector2{18, 40}, TextColor, 1);
        drawing.drawDebugText("TAB PAGE   R RESTART   P PAUSE   ESC EXIT",
            NE::Vector2{18, 58}, HintColor, 1);
    }

    NE::Camera2 camera;
    PhysicsDemo::CheckBoard board;
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
