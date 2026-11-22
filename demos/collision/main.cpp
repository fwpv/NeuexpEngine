#include "app/application.h"
#include "app/entry_point.h"
#include "app/input.h"
#include "app/platform.h"
#include "core/camera.h"
#include "core/math2d.h"
#include "graphics/renderer.h"
#include "world.h"

#include <string>

namespace NE = NeuexpEngine;

namespace
{
constexpr char WindowTitle[] = "Collision checks";
constexpr int WindowWidth = 1100;
constexpr int WindowHeight = 700;

const NE::Color Background{0.03F, 0.05F, 0.09F, 1};
const NE::Color TitleColor{0.55F, 0.85F, 0.95F, 1};
const NE::Color TextColor{0.88F, 0.9F, 0.94F, 1};
const NE::Color HintColor{0.95F, 0.8F, 0.4F, 1};

// Демонстрация — фабрика проверок: каждая зона показывает один вид распознавания
// и подписывает рядом, что обнаружено в этот момент.
class CollisionApplication final : public NE::Application
{
private:
    bool onStart(Platform::Error& error) override
    {
        if (!initializeRenderer(error)) {
            return false;
        }
        camera.referenceSize = NE::Size2{CollisionDemo::WorldWidth, CollisionDemo::WorldHeight};
        camera.position = NE::Vector2{CollisionDemo::WorldWidth / 2, CollisionDemo::WorldHeight / 2};
        camera.mode = NE::ViewportMode::Fit;
        return true;
    }

    bool onInput(const NE::FrameInfo&, Platform::Error&) override
    {
        if (Platform::isKeyDown(Platform::Key::Escape)) {
            requestExit();
        }
        if (Platform::isKeyPressed(Platform::Key::Tab)) {
            checkBoard.nextPage();
        }
        if (Platform::isKeyPressed(Platform::Key::P)) {
            setPaused(!isPaused());
        }
        if (Platform::isKeyPressed(Platform::Key::R)) {
            checkBoard.restart();
        }
        if (Platform::isKeyPressed(Platform::Key::T)) {
            checkBoard.toggleTrigger();
        }
        if (Platform::isKeyPressed(Platform::Key::L)) {
            checkBoard.toggleMask();
        }
        return true;
    }

    bool onUpdate(const NE::FrameInfo& frame, Platform::Error&) override
    {
        checkBoard.update(frame.deltaTime);
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
        checkBoard.draw(drawing);
        drawHud(drawing);
        return endFrame(error);
    }

    void drawHud(NE::Renderer2D& drawing) const
    {
        drawing.drawDebugText("COLLISION CHECKS: EVERY ZONE CHECKS ONE RULE AND LABELS ITS RESULT",
            NE::Vector2{18, 12}, TitleColor, 2);
        const std::string page = "PAGE " + std::to_string(checkBoard.pageIndex() + 1) + " OF "
            + std::to_string(checkBoard.pageCount()) + ": " + checkBoard.pageName()
            + (isPaused() ? "   PAUSED" : "");
        drawing.drawDebugText(page, NE::Vector2{18, 40}, TextColor, 1);
        drawing.drawDebugText("TAB PAGE   P PAUSE   R RESTART   T TRIGGER   L MASK   ESC EXIT",
            NE::Vector2{18, 58}, HintColor, 1);
    }

    NE::Camera2 camera;
    CollisionDemo::CheckBoard checkBoard;
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
    CollisionApplication application;
    Platform::Error failure;
    const bool success = application.run(options, failure);
    if (!success) {
        Platform::reportError(failure);
    }
    return success ? 0 : 1;
}
