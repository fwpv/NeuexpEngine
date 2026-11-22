#include "scene.h"

#include <cmath>
#include <utility>

namespace GuiDemo
{
namespace NE = NeuexpEngine;

constexpr float OrbitRadius = 110;
constexpr float OrbitCenterY = 0.63F;
constexpr float SatelliteRadius = 12;
constexpr float FlightCycleSeconds = 30;
constexpr float HudPadding = 12;
constexpr float HudSpacing = 8;
constexpr float ControlsBottomOffset = 46;
constexpr NE::Color OrbitColor{0.15F, 0.3F, 0.4F, 1};
constexpr NE::Color SatelliteColor{0.25F, 0.8F, 0.65F, 1};
constexpr NE::Color ResetButtonColor{0.2F, 0.65F, 0.4F, 1};

GameScene::GameScene(const UiResources& resources, std::function<void()> pauseRequested)
    : resources_(resources)
    , pauseRequested_(std::move(pauseRequested))
{
}

void GameScene::build()
{
    screen_ = std::make_shared<UI::Screen>();

    auto hud = screen_->addContainer(UI::Placement::horizontalCenter(NE::Size2{320, 170}));
    hud->layout() = UI::Layout{UI::Flow::Column, HudPadding, HudSpacing, UI::Align::Start};
    hud->setBackgroundVisible(true);
    hud->setPointerPolicy(UI::PointerPolicy::Block);

    elements_.text.push_back(hud->addLabel(
        UI::Placement::horizontalCenter(NE::Size2{280, 24}), u8"ВРЕМЯ ПОЛЁТА"));

    counter_ = hud->addNumericIndicator(
        UI::Placement::horizontalCenter(NE::Size2{280, 26}));
    elements_.text.push_back(counter_);

    progress_ = hud->addProgressBar(
        UI::Placement::horizontalCenter(NE::Size2{280, 18}));

    auto pauseButton = hud->addButton(
        UI::Placement::horizontalCenter(buttonSize(resources_, 180)), u8"Пауза");
    elements_.buttons.push_back(pauseButton);
    elements_.text.push_back(pauseButton);
    subscriptions_.push_back(pauseButton->subscribe(UI::EventType::Click, [this] {
        pauseRequested_();
    }));

    auto controls = screen_->addContainer(
        UI::Placement::horizontalCenter(NE::Size2{352, 60}));
    UI::Placement placement = controls->placement();
    placement.vertical = UI::Align::End;
    placement.offset.y = -ControlsBottomOffset;
    controls->setPlacement(placement);
    controls->layout() = UI::Layout{UI::Flow::Row, 8, 8, UI::Align::Center};

    auto restart = controls->addButton(
        UI::Placement::horizontalCenter(buttonSize(resources_, 64)));
    elements_.buttons.push_back(restart);
    elements_.text.push_back(restart);
    restart->style().background = ResetButtonColor;
    subscriptions_.push_back(restart->subscribe(UI::EventType::Click, [this] {
        elapsed_ = 0;
    }));

    elements_.text.push_back(controls->addLabel(
        UI::Placement::horizontalCenter(NE::Size2{110, 32}), u8"Сброс"));

    auto orbit = controls->addCheckbox(
        UI::Placement::horizontalCenter(NE::Size2{24, 32}));
    elements_.checkboxes.push_back(orbit);
    elements_.text.push_back(orbit);
    orbit->setChecked(true);
    subscriptions_.push_back(orbit->subscribe(UI::EventType::Change, [this](const UI::Event& event) {
        showOrbit_ = event.checked;
    }));

    elements_.text.push_back(controls->addLabel(
        UI::Placement::horizontalCenter(NE::Size2{100, 32}), u8"Орбита"));

    hud_.setScreen(screen_);
}

void GameScene::reset()
{
    elapsed_ = 0;
}

void GameScene::update(float deltaTime)
{
    elapsed_ += deltaTime;
    counter_->setValue(elapsed_, 1);
    progress_->setValue(std::fmod(elapsed_, FlightCycleSeconds) / FlightCycleSeconds);
}

bool GameScene::handle(const NE::Event& event)
{
    return hud_.handle(event);
}

bool GameScene::setViewport(const NE::Viewport& viewport, float scale)
{
    if (!hud_.setViewport(viewport, scale)) {
        return false;
    }
    NE::UiCanvas canvas;
    NE::UiError canvasError = NE::UiError::None;
    return canvas.createView(viewport, view_, canvasError);
}

bool GameScene::draw(NE::Renderer2D& renderer, bool withHud, NE::ResourceError& error)
{
    renderer.setUiView(view_);
    drawOrbit(renderer);
    if (!withHud) {
        return true;
    }
    return hud_.draw(renderer, error);
}

void GameScene::applyStyle(const Settings& settings)
{
    GuiDemo::applyStyle(resources_, settings, elements_);
}

void GameScene::drawOrbit(NE::Renderer2D& renderer) const
{
    const NE::Vector2 center{view_.size.width / 2, view_.size.height * OrbitCenterY};
    if (showOrbit_) {
        renderer.drawCircle(NE::Circle{center, OrbitRadius}, OrbitColor, NE::ShapeFill::Outline);
    }
    const NE::Vector2 satellite{center.x + OrbitRadius * std::cos(elapsed_),
        center.y + OrbitRadius * std::sin(elapsed_)};
    renderer.drawCircle(NE::Circle{satellite, SatelliteRadius}, SatelliteColor);
}
}
