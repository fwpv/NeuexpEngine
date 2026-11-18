#pragma once

#include "camera.h"
#include "gui/context.h"
#include "input.h"
#include "renderer.h"
#include "resources.h"
#include "settings.h"
#include "ui.h"
#include "ui_theme.h"

#include <functional>
#include <memory>
#include <vector>

namespace GuiDemo
{
// Игровая сцена: орбита и собственный HUD.
// Сцена не знает о меню; о паузе она сообщает
// приложению через callback.
class GameScene
{
public:
    GameScene(const UiResources& resources, std::function<void()> pauseRequested);

    void build();
    void reset();
    void update(float deltaTime);
    bool handle(const NE::Event& event);
    bool setViewport(const NE::Viewport& viewport, float scale);
    // withHud = false скрывает HUD, пока сцену перекрывает меню настроек.
    bool draw(NE::Renderer2D& renderer, bool withHud, NE::ResourceError& error);
    void applyStyle(const Settings& settings);

private:
    void drawOrbit(NE::Renderer2D& renderer) const;

    const UiResources& resources_;
    std::function<void()> pauseRequested_;

    UI::Context hud_;
    std::shared_ptr<UI::Screen> screen_;
    std::shared_ptr<UI::NumericIndicator> counter_;
    std::shared_ptr<UI::ProgressBar> progress_;
    UiElements elements_;
    std::vector<UI::Subscription> subscriptions_;

    NE::UiView view_;
    float elapsed_ = 0;
    bool showOrbit_ = true;
};
}
