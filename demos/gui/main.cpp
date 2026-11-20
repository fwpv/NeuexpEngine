#include "menu.h"
#include "scene.h"
#include "settings.h"
#include "ui_theme.h"

#include "app/application.h"
#include "app/entry_point.h"
#include "gui/context.h"
#include "app/input.h"
#include "app/platform.h"
#include "graphics/renderer.h"
#include "core/resources.h"
#include "core/texture.h"
#include "graphics/ui_view.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace NE = NeuexpEngine;
namespace UI = NE::Gui;

constexpr char WindowTitle[] = "GUI / Orbital menu";
constexpr char ErrorTitle[] = "GUI demo";
constexpr float StatusBarHeight = 34;
constexpr NE::Color SceneBackground{0.025F, 0.045F, 0.075F, 1};

enum class Mode
{
    Menu,
    Playing,
    Paused
};

// Приложение связывает окно, меню и игровую сцену:
// выбирает активный экран, передаёт ему ввод,
// обновление и отрисовку, а общие ресурсы загружает один раз.
class GuiDemoApplication final : public NE::Application
{
private:
    bool onStart(Platform::Error& error) override
    {
        if (!initializeRenderer(error) || !initializeWindow(error) || !initializePaths(error)) {
            return false;
        }
        const std::string settingsProblem = loadSettingsFile();
        const std::string resourcesProblem = loadResources();
        bool vsyncAvailable = false;
        const std::string vsyncProblem = applySavedVSync(vsyncAvailable);

        menu_ = std::make_unique<GuiDemo::Menu>(settings_, resources_, settingsPath_, vsyncAvailable,
            makeMenuCallbacks());
        menu_->build();
        scene_ = std::make_unique<GuiDemo::GameScene>(resources_, [this] { pauseGame(); });
        scene_->build();
        scene_->applyStyle(settings_);
        buildStatusBar();
        mode_ = Mode::Menu;
        menu_->showMain();

        if (!settingsProblem.empty()) {
            showMessage(settingsProblem);
        }
        if (!resourcesProblem.empty()) {
            showMessage(resourcesProblem);
        }
        if (!vsyncProblem.empty()) {
            showMessage(vsyncProblem);
        }
        return true;
    }

    bool onInput(const NE::FrameInfo& frame, Platform::Error& error) override
    {
        if (!configure(frame, error)) {
            return false;
        }
        for (const NE::Event& event : Platform::getInput().events()) {
            if (mode_ == Mode::Playing) {
                if (scene_->handle(event)) {
                    continue;
                }
                if (event.type == NE::EventType::KeyPressed && event.key == NE::Key::Escape
                    && !event.repeat) {
                    pauseGame();
                }
            } else {
                menu_->handle(event);
            }
        }
        return true;
    }

    bool onUpdate(const NE::FrameInfo& frame, Platform::Error&) override
    {
        if (mode_ == Mode::Playing) {
            scene_->update(frame.deltaTime);
        }
        return true;
    }

    bool onRender(const NE::FrameInfo& frame, Platform::Error& error) override
    {
        beginFrame(frame.window.width, frame.window.height, SceneBackground);
        auto& renderer = getRenderer();
        NE::ResourceError failure;
        bool drawn = true;
        if (mode_ != Mode::Menu) {
            const bool withHud = mode_ == Mode::Playing || !menu_->settingsOpen();
            drawn = scene_->draw(renderer, withHud, failure);
        }
        if (drawn && mode_ != Mode::Playing) {
            drawn = menu_->draw(renderer, failure);
        }
        if (drawn) {
            drawn = statusGui_.draw(renderer, failure);
        }
        const bool finished = endFrame(error);
        if (!drawn) {
            error = Platform::Error{ErrorTitle, failure.message, {}};
        }
        return drawn && finished;
    }

    bool initializeWindow(Platform::Error& error)
    {
        Platform::WindowState window;
        return Platform::getWindowState(window, error);
    }

    bool initializePaths(Platform::Error& error)
    {
        NE::ResourceError failure;
        std::filesystem::path executable;
        if (!executableDirectory(executable, failure)) {
            error = Platform::Error{ErrorTitle, failure.message, {}};
            return false;
        }
        settingsPath_ = executable / GuiDemo::SettingsFileName;
        return true;
    }

    // Ошибка чтения не мешает запуску:
    // используются начальные настройки.
    std::string loadSettingsFile()
    {
        std::string error;
        if (!GuiDemo::loadSettings(settingsPath_, settings_, error)) {
            return error;
        }
        return {};
    }

    std::string loadResources()
    {
        NE::ResourceFiles files;
        NE::ResourceError failure;
        if (!files.initialize(failure)) {
            resources_.skinReady = false;
            settings_.images = false;
            settings_.bitmapText = false;
            return failure.message;
        }
        std::string problem;
        std::string skinError;
        if (!loadSkin(files, skinError)) {
            problem = skinError;
        }
        std::string fontError;
        if (!loadFont(files, fontError)) {
            if (!problem.empty()) {
                problem += " / ";
            }
            problem += fontError;
        }
        if (!resources_.skinReady) {
            settings_.images = false;
        }
        if (!resources_.font.isValid()) {
            settings_.bitmapText = false;
        }
        return problem;
    }

    bool loadSkin(const NE::ResourceFiles& files, std::string& error)
    {
        NE::ResourceError failure;
        NE::Texture texture;
        if (!getRenderer().loadTexture(files, "gui/btns.png", texture, failure)) {
            error = failure.message;
            return false;
        }
        if (!resources_.atlas.load(texture, files, "gui/gui_atlas.txt", failure)) {
            error = failure.message;
            return false;
        }
        if (!resources_.skin.load(resources_.atlas)) {
            error = "Cannot build the GUI skin from the atlas.";
            return false;
        }
        resources_.skinReady = true;
        return true;
    }

    bool loadFont(const NE::ResourceFiles& files, std::string& error)
    {
        NE::ResourceError failure;
        if (!getRenderer().loadFont(files, "font/fnt14.fnt", resources_.font, failure)) {
            error = failure.message;
            return false;
        }
        return true;
    }

    std::string applySavedVSync(bool& available)
    {
        Platform::Error failure;
        if (setVSync(settings_.vsync, failure)) {
            available = true;
            return {};
        }
        available = false;
        settings_.vsync = false;
        return failure.message;
    }

    GuiDemo::MenuCallbacks makeMenuCallbacks()
    {
        GuiDemo::MenuCallbacks callbacks;
        callbacks.startGame = [this] { startGame(); };
        callbacks.resumeGame = [this] { resumeGame(); };
        callbacks.goToMainMenu = [this] { goToMainMenu(); };
        callbacks.exitApplication = [this] { requestExit(); };
        callbacks.settingsApplied = [this] { scene_->applyStyle(settings_); };
        callbacks.setVSync = [this](bool enabled, std::string& error) {
            Platform::Error failure;
            if (!setVSync(enabled, failure)) {
                error = failure.message;
                return false;
            }
            return true;
        };
        callbacks.showMessage = [this](const std::string& text) { showMessage(text); };
        return callbacks;
    }

    void buildStatusBar()
    {
        const std::string text = u8"Esc: назад / пауза. Колесо: прокрутка / "
            + std::string(GuiDemo::SettingsFileName);

        // Подпись статусной строки
        status_ = std::make_shared<UI::Label>(text);
        UI::Placement placement;
        placement.size = NE::Size2{0, StatusBarHeight};
        placement.horizontal = UI::Align::Stretch;
        placement.vertical = UI::Align::End;
        placement.scaleMode = UI::ScaleMode::Pixels;
        status_->setPlacement(placement);
        status_->style().debugPixel = 1;

        // Экран статусной строки
        auto footer = std::make_shared<UI::Screen>();
        footer->add(status_);
        statusGui_.setScreen(footer);
    }

    bool configure(const NE::FrameInfo& frame, Platform::Error& error)
    {
        if (frame.window.minimized) {
            return true;
        }
        const NE::Viewport viewport{0, 0, frame.window.width, frame.window.height};
        const float scale = menu_->scale();
        if (!menu_->setViewport(viewport, scale)) {
            error = Platform::Error{ErrorTitle, "Cannot configure the menu viewport.", {}};
            return false;
        }
        if (!scene_->setViewport(viewport, scale)) {
            error = Platform::Error{ErrorTitle, "Cannot configure the scene viewport.", {}};
            return false;
        }
        if (!statusGui_.setViewport(viewport)) {
            error = Platform::Error{ErrorTitle, "Cannot configure the status bar viewport.", {}};
            return false;
        }
        return true;
    }

    void showMessage(const std::string& text)
    {
        status_->setText(text);
        std::cerr << text << '\n';
    }

    void startGame()
    {
        mode_ = Mode::Playing;
        setPaused(false);
        scene_->reset();
    }

    void pauseGame()
    {
        mode_ = Mode::Paused;
        setPaused(true);
        menu_->showPause();
    }

    void resumeGame()
    {
        mode_ = Mode::Playing;
        setPaused(false);
    }

    void goToMainMenu()
    {
        mode_ = Mode::Menu;
        setPaused(false);
        menu_->showMain();
    }

    GuiDemo::Settings settings_;
    GuiDemo::UiResources resources_;
    std::filesystem::path settingsPath_;
    std::unique_ptr<GuiDemo::Menu> menu_;
    std::unique_ptr<GuiDemo::GameScene> scene_;
    UI::Context statusGui_;
    std::shared_ptr<UI::Label> status_;
    Mode mode_ = Mode::Menu;
};

int NeuexpEngine::applicationMain()
{
    GuiDemoApplication application;
    NE::ApplicationOptions options;
    options.window.title = WindowTitle;
    options.window.width = 900;
    options.window.height = 700;
    options.window.minimumWidth = 480;
    options.window.minimumHeight = 360;
    Platform::Error error;
    if (!application.run(options, error)) {
        Platform::reportError(error);
        return 1;
    }
    return 0;
}
