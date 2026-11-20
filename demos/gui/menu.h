#pragma once

#include "core/camera.h"
#include "gui/context.h"
#include "app/input.h"
#include "graphics/renderer.h"
#include "core/resources.h"
#include "settings.h"
#include "ui_theme.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace GuiDemo
{
// Действия меню; все обработчики заполняет приложение.
struct MenuCallbacks
{
    std::function<void()> startGame;
    std::function<void()> resumeGame;
    std::function<void()> goToMainMenu;
    std::function<void()> exitApplication;
    std::function<void()> settingsApplied;
    std::function<bool(bool, std::string&)> setVSync;
    std::function<void(const std::string&)> showMessage;
};

// Экраны меню, настроек и паузы. Класс владеет своим GUI-контекстом
// и сообщает приложению о действиях пользователя через MenuCallbacks.
class Menu
{
public:
    Menu(Settings& settings, const UiResources& resources, std::filesystem::path settingsPath,
        bool vsyncAvailable, MenuCallbacks callbacks);

    void build();
    void showMain();
    void showPause();
    bool handle(const NE::Event& event);
    bool draw(NE::Renderer2D& renderer, NE::ResourceError& error);
    bool setViewport(const NE::Viewport& viewport, float scale);
    bool settingsOpen() const;
    // Текущий масштаб интерфейса,
    // включая ещё не применённый предпросмотр.
    float scale() const;

private:
    // Панель экрана и её исходный логический размер:
    // окно может уменьшить панель при изменении размера.
    struct Panel
    {
        std::shared_ptr<UI::Container> element;
        NE::Size2 designSize;
    };

    void buildMainMenu();
    void buildSettingsPage();
    void buildPausePage();
    void applyStyle();
    void openSettings();
    void closeSettings();
    void applySettings();
    void changeScale(float delta);

    std::shared_ptr<UI::Container> panel(const std::shared_ptr<UI::Screen>& page,
        NE::Size2 designSize);

    Settings& settings_;
    const UiResources& resources_;
    std::filesystem::path settingsPath_;
    MenuCallbacks callbacks_;
    bool vsyncAvailable_ = false;

    UI::Context gui_;
    std::shared_ptr<UI::Screen> mainScreen_;
    std::shared_ptr<UI::Screen> settingsScreen_;
    std::shared_ptr<UI::Screen> pauseScreen_;
    std::shared_ptr<UI::NumericIndicator> scaleValue_;
    std::shared_ptr<UI::Checkbox> vsync_;
    std::shared_ptr<UI::Checkbox> images_;
    std::shared_ptr<UI::Checkbox> font_;
    std::shared_ptr<UI::Checkbox> textFiltering_;
    UiElements elements_;
    std::vector<Panel> panels_;
    std::vector<UI::Subscription> subscriptions_;

    Settings draft_;
    bool paused_ = false;
    bool settingsFromPause_ = false;
};
}
