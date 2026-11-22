#include "menu.h"

#include <algorithm>
#include <string>
#include <utility>

namespace GuiDemo
{
namespace NE = NeuexpEngine;

constexpr int PanelHorizontalMargin = 16;
constexpr int PanelVerticalMargin = 84;
constexpr float PanelPadding = 14;
constexpr float PanelSpacing = 8;

Menu::Menu(Settings& settings, const UiResources& resources, std::filesystem::path settingsPath,
    bool vsyncAvailable, MenuCallbacks callbacks)
    : settings_(settings)
    , resources_(resources)
    , settingsPath_(std::move(settingsPath))
    , callbacks_(std::move(callbacks))
    , vsyncAvailable_(vsyncAvailable)
    , draft_(settings)
{
}

void Menu::build()
{
    buildMainMenu();
    buildSettingsPage();
    buildPausePage();
    applyStyle();
}

void Menu::showMain()
{
    paused_ = false;
    gui_.setScreen(mainScreen_);
}

void Menu::showPause()
{
    paused_ = true;
    gui_.setScreen(pauseScreen_);
}

bool Menu::handle(const NE::Event& event)
{
    if (gui_.handle(event)) {
        return true;
    }
    if (event.type == NE::EventType::KeyPressed && event.key == NE::Key::Escape && !event.repeat) {
        if (settingsOpen()) {
            closeSettings();
            return true;
        }
        if (paused_) {
            callbacks_.resumeGame();
            return true;
        }
        callbacks_.exitApplication();
        return true;
    }
    return false;
}

bool Menu::draw(NE::Renderer2D& renderer, NE::ResourceError& error)
{
    return gui_.draw(renderer, error);
}

bool Menu::setViewport(const NE::Viewport& viewport, float scale)
{
    const float availableWidth = std::max(0.0F,
        (static_cast<float>(viewport.width) - PanelHorizontalMargin) / scale);
    const float availableHeight = std::max(0.0F,
        (static_cast<float>(viewport.height) - PanelVerticalMargin) / scale);
    for (Panel& panel : panels_) {
        UI::Placement placement = panel.element->placement();
        placement.size.width = std::min(panel.designSize.width, availableWidth);
        placement.size.height = std::min(panel.designSize.height, availableHeight);
        panel.element->setPlacement(placement);
    }
    return gui_.setViewport(viewport, scale);
}

bool Menu::settingsOpen() const
{
    return gui_.screen() == settingsScreen_;
}

float Menu::scale() const
{
    return draft_.scale;
}

void Menu::buildMainMenu()
{
    mainScreen_ = std::make_shared<UI::Screen>();

    auto mainPanel = panel(mainScreen_, NE::Size2{340, 340});

    elements_.text.push_back(mainPanel->addLabel(
        UI::Placement::horizontalCenter(NE::Size2{310, 34}), u8"ОРБИТА / МЕНЮ"));

    auto start = mainPanel->addButton(
        UI::Placement::horizontalCenter(buttonSize(resources_, 230)), u8"Начать");
    elements_.buttons.push_back(start);
    elements_.text.push_back(start);
    subscriptions_.push_back(start->subscribe(UI::EventType::Click, [this] {
        callbacks_.startGame();
    }));

    auto options = mainPanel->addButton(
        UI::Placement::horizontalCenter(buttonSize(resources_, 230)), u8"Настройки");
    elements_.buttons.push_back(options);
    elements_.text.push_back(options);
    subscriptions_.push_back(options->subscribe(UI::EventType::Click, [this] {
        openSettings();
    }));

    auto exit = mainPanel->addButton(
        UI::Placement::horizontalCenter(buttonSize(resources_, 230)), u8"Выход");
    elements_.buttons.push_back(exit);
    elements_.text.push_back(exit);
    subscriptions_.push_back(exit->subscribe(UI::EventType::Click, [this] {
        callbacks_.exitApplication();
    }));
}

void Menu::buildSettingsPage()
{
    settingsScreen_ = std::make_shared<UI::Screen>();

    auto settingsPanel = panel(settingsScreen_, NE::Size2{360, 560});

    elements_.text.push_back(settingsPanel->addLabel(
        UI::Placement::horizontalCenter(NE::Size2{320, 32}), u8"НАСТРОЙКИ"));

    auto scaleRow = settingsPanel->addContainer(
        UI::Placement::horizontalCenter(NE::Size2{320, 41}));
    scaleRow->layout() = UI::Layout{UI::Flow::Row, 0, 8, UI::Align::Center};

    auto less = scaleRow->addButton(
        UI::Placement::horizontalCenter(buttonSize(resources_, 96)), "-");
    elements_.buttons.push_back(less);
    elements_.text.push_back(less);
    subscriptions_.push_back(less->subscribe(UI::EventType::Click, [this] {
        changeScale(-0.25F);
    }));

    scaleValue_ = scaleRow->addNumericIndicator(
        UI::Placement::horizontalCenter(NE::Size2{96, 41}));
    elements_.text.push_back(scaleValue_);

    auto more = scaleRow->addButton(
        UI::Placement::horizontalCenter(buttonSize(resources_, 96)), "+");
    elements_.buttons.push_back(more);
    elements_.text.push_back(more);
    subscriptions_.push_back(more->subscribe(UI::EventType::Click, [this] {
        changeScale(0.25F);
    }));

    elements_.text.push_back(settingsPanel->addLabel(
        UI::Placement::horizontalCenter(NE::Size2{320, 24}), u8"Масштаб интерфейса"));

    vsync_ = settingsPanel->addCheckbox(
        UI::Placement::horizontalCenter(NE::Size2{310, 30}), "VSync");
    elements_.checkboxes.push_back(vsync_);
    elements_.text.push_back(vsync_);
    vsync_->setEnabled(vsyncAvailable_);
    subscriptions_.push_back(vsync_->subscribe(UI::EventType::Change, [this](const UI::Event& event) {
        draft_.vsync = event.checked;
    }));

    images_ = settingsPanel->addCheckbox(
        UI::Placement::horizontalCenter(NE::Size2{310, 30}), u8"Изображения кнопок");
    elements_.checkboxes.push_back(images_);
    elements_.text.push_back(images_);
    images_->setEnabled(resources_.skinReady);
    subscriptions_.push_back(images_->subscribe(UI::EventType::Change, [this](const UI::Event& event) {
        draft_.images = event.checked;
        applyStyle();
    }));

    font_ = settingsPanel->addCheckbox(
        UI::Placement::horizontalCenter(NE::Size2{310, 30}), u8"Растровый шрифт");
    elements_.checkboxes.push_back(font_);
    elements_.text.push_back(font_);
    font_->setEnabled(resources_.font.isValid());
    subscriptions_.push_back(font_->subscribe(UI::EventType::Change, [this](const UI::Event& event) {
        draft_.bitmapText = event.checked;
        applyStyle();
    }));

    textFiltering_ = settingsPanel->addCheckbox(
        UI::Placement::horizontalCenter(NE::Size2{310, 30}), u8"Фильтрация текста");
    elements_.checkboxes.push_back(textFiltering_);
    elements_.text.push_back(textFiltering_);
    subscriptions_.push_back(textFiltering_->subscribe(UI::EventType::Change,
        [this](const UI::Event& event) {
            draft_.textFiltering = event.checked;
            applyStyle();
        }));

    auto apply = settingsPanel->addButton(
        UI::Placement::horizontalCenter(buttonSize(resources_, 250)), u8"Применить");
    elements_.buttons.push_back(apply);
    elements_.text.push_back(apply);
    subscriptions_.push_back(apply->subscribe(UI::EventType::Click, [this] {
        applySettings();
    }));

    auto cancel = settingsPanel->addButton(
        UI::Placement::horizontalCenter(buttonSize(resources_, 250)), u8"Отмена");
    elements_.buttons.push_back(cancel);
    elements_.text.push_back(cancel);
    subscriptions_.push_back(cancel->subscribe(UI::EventType::Click, [this] {
        closeSettings();
    }));

    const std::string hintText = u8"Настройки сохраняются\nрядом с демо";
    auto hint = settingsPanel->addLabel(
        UI::Placement::horizontalCenter(NE::Size2{320, 56}), hintText);
    elements_.text.push_back(hint);
    hint->style().debugPixel = 2;
}

void Menu::buildPausePage()
{
    pauseScreen_ = std::make_shared<UI::Screen>();

    auto pausePanel = panel(pauseScreen_, NE::Size2{340, 320});

    elements_.text.push_back(pausePanel->addLabel(
        UI::Placement::horizontalCenter(NE::Size2{310, 30}), u8"ПАУЗА"));

    auto resume = pausePanel->addButton(
        UI::Placement::horizontalCenter(buttonSize(resources_, 230)), u8"Продолжить");
    elements_.buttons.push_back(resume);
    elements_.text.push_back(resume);
    subscriptions_.push_back(resume->subscribe(UI::EventType::Click, [this] {
        callbacks_.resumeGame();
    }));

    auto options = pausePanel->addButton(
        UI::Placement::horizontalCenter(buttonSize(resources_, 230)), u8"Настройки");
    elements_.buttons.push_back(options);
    elements_.text.push_back(options);
    subscriptions_.push_back(options->subscribe(UI::EventType::Click, [this] {
        openSettings();
    }));

    auto home = pausePanel->addButton(
        UI::Placement::horizontalCenter(buttonSize(resources_, 230)), u8"Главное меню");
    elements_.buttons.push_back(home);
    elements_.text.push_back(home);
    subscriptions_.push_back(home->subscribe(UI::EventType::Click, [this] {
        callbacks_.goToMainMenu();
    }));
}

void Menu::applyStyle()
{
    textFiltering_->setEnabled(draft_.bitmapText && resources_.font.isValid());
    GuiDemo::applyStyle(resources_, draft_, elements_);
}

void Menu::openSettings()
{
    draft_ = settings_;
    vsync_->setChecked(draft_.vsync);
    images_->setChecked(draft_.images);
    font_->setChecked(draft_.bitmapText);
    textFiltering_->setChecked(draft_.textFiltering);
    scaleValue_->setValue(draft_.scale, 2);
    applyStyle();
    settingsFromPause_ = paused_;
    gui_.setScreen(settingsScreen_);
}

void Menu::closeSettings()
{
    draft_ = settings_;
    applyStyle();
    gui_.setScreen(settingsFromPause_ ? pauseScreen_ : mainScreen_);
}

void Menu::applySettings()
{
    if (draft_.vsync != settings_.vsync) {
        std::string error;
        if (!callbacks_.setVSync(draft_.vsync, error)) {
            callbacks_.showMessage(error);
            return;
        }
    }
    std::string error;
    if (!saveSettings(settingsPath_, draft_, error)) {
        std::string rollbackError;
        if (!callbacks_.setVSync(settings_.vsync, rollbackError)) {
            error += " / " + rollbackError;
        }
        callbacks_.showMessage(error);
        return;
    }
    settings_ = draft_;
    callbacks_.settingsApplied();
    callbacks_.showMessage(u8"Настройки сохранены / " + std::string(SettingsFileName));
    closeSettings();
}

void Menu::changeScale(float delta)
{
    draft_.scale = std::clamp(draft_.scale + delta, MinScale, MaxScale);
    scaleValue_->setValue(draft_.scale, 2);
}

std::shared_ptr<UI::Container> Menu::panel(const std::shared_ptr<UI::Screen>& page,
    NE::Size2 designSize)
{
    auto result = page->addContainer(UI::Placement::horizontalCenter(designSize));
    UI::Placement placement = result->placement();
    placement.vertical = UI::Align::Center;
    result->setPlacement(placement);
    result->layout() = UI::Layout{UI::Flow::Column, PanelPadding, PanelSpacing, UI::Align::Center};
    result->setBackgroundVisible(true);
    result->setScrollable(true);
    result->setPointerPolicy(UI::PointerPolicy::Block);
    panels_.push_back(Panel{result, designSize});
    return result;
}
}
