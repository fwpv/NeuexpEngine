#include "settings.h"
#include "application.h"
#include "atlas.h"
#include "entry_point.h"
#include "gui/context.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <array>

namespace
{
using namespace NeuexpEngine;
namespace UI = NeuexpEngine::Gui;

class MenuDemo final : public Application
{
private:
    template<class T, class... Args>
    std::shared_ptr<T> add(const std::shared_ptr<UI::Container>& parent, Size2 size, Args&&... args)
    {
        auto element = std::make_shared<T>(std::forward<Args>(args)...);
        UI::Placement placement;
        placement.size = size;
        placement.horizontal = UI::Align::Center;
        element->setPlacement(placement);
        parent->add(element);
        styled.push_back(element);
        return element;
    }

    std::shared_ptr<UI::Button> addButton(const std::shared_ptr<UI::Container>& parent,
        float width, const std::string& text = {})
    {
        // Размер элемента и его области ввода сохраняет пропорции выбранного скина.
        const auto region = atlas.find(width < 100 ? "btn2_normal" : "btn1_normal");
        const Size2 imageSize = region ? region->size : Size2{width < 100 ? 96.0F : 150.0F, 41};
        const float buttonWidth = width * 0.89F;
        return add<UI::Button>(parent, Size2{buttonWidth, buttonWidth * imageSize.height / imageSize.width}, text);
    }

    std::shared_ptr<UI::Container> panel(const std::shared_ptr<UI::Screen>& page, Size2 size)
    {
        auto result = add<UI::Container>(page, size);
        auto placement = result->placement();
        placement.vertical = UI::Align::Center;
        result->setPlacement(placement);
        result->layout() = UI::Layout{UI::Flow::Column, 14, 8, UI::Align::Center};
        result->setBackgroundVisible(true);
        result->setScrollable(true);
        panels.push_back(result);
        panelSizes.push_back(size);
        result->setPointerPolicy(UI::PointerPolicy::Block);
        return result;
    }

    template<class Handler>
    void click(const std::shared_ptr<UI::Button>& button, Handler handler)
    {
        subscriptions.push_back(button->subscribe(UI::EventType::Click, [handler](const UI::Event&) { handler(); }));
    }

    void message(const std::string& text)
    {
        status->setText(text);
        std::cerr << text << '\n';
    }

    bool onStart(Platform::Error& error) override
    {
        if (!initializeRenderer(error)) { return false; }
        Platform::WindowState window;
        if (!Platform::getWindowState(window, error)) { return false; }
        applied.scale = std::clamp(window.systemScale, 0.75F, 2.0F);
        ResourceFiles files;
        ResourceError failure;
        const bool assetsAvailable = files.initialize(failure);
        std::string assetError = assetsAvailable ? std::string{} : failure.message;
        std::filesystem::path executablePath;
        if (!executableDirectory(executablePath, failure)) {
            error = Platform::Error{"GUI demo", failure.message, {}};
            return false;
        }
        settingsPath = executablePath / "gui_demo_settings.tmp";
        std::string loadError;
        GuiDemo::loadSettings(settingsPath, applied, loadError);
        draft = applied;
        // Ассеты необязательны; ошибки доступного оформления видны в статусе и stderr.
        Texture texture;
        if (assetsAvailable && getRenderer().loadTexture(files, "gui/btns.png", texture, failure)
            && atlas.load(texture, files, "gui/gui_atlas.txt", failure)) {
            imagesAvailable = true;
        } else if (assetsAvailable) {
            assetError = failure.message;
        }
        if (assetsAvailable && !getRenderer().loadFont(files, "font/fnt14.fnt", bitmap, failure)) {
            if (!assetError.empty()) { assetError += " / "; }
            assetError += failure.message;
        }
        if (!imagesAvailable) { applied.images = draft.images = false; }
        if (!bitmap.isValid()) { applied.bitmapText = draft.bitmapText = false; }
        buildPages();
        applyStyle();
        if (!loadError.empty()) { message(loadError); }
        if (!assetError.empty()) { message(assetError); }
        Platform::Error vsyncError;
        if (!setVSync(applied.vsync, vsyncError)) {
            applied.vsync = draft.vsync = false;
            vsync->setEnabled(false);
            message(vsyncError.message);
        }
        gui.setScreen(mainMenu);
        return true;
    }

    void buildPages()
    {
        mainMenu = std::make_shared<UI::Screen>();
        settings = std::make_shared<UI::Screen>();
        game = std::make_shared<UI::Screen>();
        pause = std::make_shared<UI::Screen>();
        auto mainPanel = panel(mainMenu, Size2{340, 340});
        add<UI::Label>(mainPanel, Size2{310, 34}, u8"ОРБИТА / МЕНЮ");
        auto start = addButton(mainPanel, 230, u8"Начать");
        click(start, [this] {
            elapsed = 0;
            playing = true;
            gui.setScreen(game);
            setPaused(false);
        });
        auto options = addButton(mainPanel, 230, u8"Настройки");
        click(options, [this] { openSettings(); });
        auto exit = addButton(mainPanel, 230, u8"Выход");
        click(exit, [this] { requestExit(); });

        auto settingsPanel = panel(settings, Size2{360, 560});
        add<UI::Label>(settingsPanel, Size2{320, 32}, u8"НАСТРОЙКИ");
        auto scaleRow = add<UI::Container>(settingsPanel, Size2{320, 41});
        scaleRow->layout() = UI::Layout{UI::Flow::Row, 0, 8, UI::Align::Center};
        auto less = addButton(scaleRow, 96, "-");
        scaleValue = add<UI::NumericIndicator>(scaleRow, Size2{96, 41});
        auto more = addButton(scaleRow, 96, "+");
        click(less, [this] { changeScale(-0.25F); });
        click(more, [this] { changeScale(0.25F); });
        add<UI::Label>(settingsPanel, Size2{320, 24}, u8"Масштаб интерфейса");
        vsync = add<UI::Checkbox>(settingsPanel, Size2{310, 30}, "VSync");
        images = add<UI::Checkbox>(settingsPanel, Size2{310, 30}, u8"Изображения кнопок");
        font = add<UI::Checkbox>(settingsPanel, Size2{310, 30}, u8"Растровый шрифт");
        textFiltering = add<UI::Checkbox>(settingsPanel, Size2{310, 30}, u8"Фильтрация текста");
        images->setEnabled(imagesAvailable);
        font->setEnabled(bitmap.isValid());
        subscriptions.push_back(vsync->subscribe(UI::EventType::Change, [this](const UI::Event& event) {
            draft.vsync = event.checked;
        }));
        subscriptions.push_back(images->subscribe(UI::EventType::Change, [this](const UI::Event& event) {
            draft.images = event.checked;
            applyStyle();
        }));
        subscriptions.push_back(font->subscribe(UI::EventType::Change, [this](const UI::Event& event) {
            draft.bitmapText = event.checked;
            applyStyle();
        }));
        subscriptions.push_back(textFiltering->subscribe(UI::EventType::Change, [this](const UI::Event& event) {
            draft.textFiltering = event.checked;
            applyStyle();
        }));
        auto apply = addButton(settingsPanel, 250, u8"Применить");
        click(apply, [this] { applySettings(); });
        auto cancel = addButton(settingsPanel, 250, u8"Отмена");
        click(cancel, [this] { closeSettings(); });
        auto hint = add<UI::Label>(settingsPanel, Size2{320, 56}, u8"Настройки сохраняются\nрядом с демо");
        hint->style().debugPixel = 2;

        auto hud = add<UI::Container>(game, Size2{320, 170});
        hud->layout() = UI::Layout{UI::Flow::Column, 12, 8, UI::Align::Start};
        hud->setBackgroundVisible(true);
        hud->setPointerPolicy(UI::PointerPolicy::Block);
        add<UI::Label>(hud, Size2{280, 24}, u8"ВРЕМЯ ПОЛЁТА");
        counter = add<UI::NumericIndicator>(hud, Size2{280, 26});
        progress = add<UI::ProgressBar>(hud, Size2{280, 18});
        auto pauseButton = addButton(hud, 180, u8"Пауза");
        click(pauseButton, [this] { pauseGame(); });
        auto controls = add<UI::Container>(game, Size2{352, 60});
        auto controlsPlacement = controls->placement();
        controlsPlacement.vertical = UI::Align::End;
        controlsPlacement.offset.y = -46;
        controls->setPlacement(controlsPlacement);
        controls->layout() = UI::Layout{UI::Flow::Row, 8, 8, UI::Align::Center};
        auto restart = addButton(controls, 64);
        restart->style().background = Color{0.2F, 0.65F, 0.4F, 1};
        click(restart, [this] { elapsed = 0; });
        add<UI::Label>(controls, Size2{110, 32}, u8"Сброс");
        auto orbit = add<UI::Checkbox>(controls, Size2{24, 32});
        orbit->setChecked(true);
        subscriptions.push_back(orbit->subscribe(UI::EventType::Change, [this](const UI::Event& event) {
            showOrbit = event.checked;
        }));
        add<UI::Label>(controls, Size2{100, 32}, u8"Орбита");
        auto pausePanel = panel(pause, Size2{340, 320});
        add<UI::Label>(pausePanel, Size2{310, 30}, u8"ПАУЗА");
        auto resume = addButton(pausePanel, 230, u8"Продолжить");
        click(resume, [this] { gui.setModal({}); setPaused(false); });
        auto pauseOptions = addButton(pausePanel, 230, u8"Настройки");
        click(pauseOptions, [this] { openSettings(); });
        auto home = addButton(pausePanel, 230, u8"Главное меню");
        click(home, [this] {
            playing = false;
            gui.setScreen(mainMenu);
            setPaused(false);
        });

        // Эта строка остаётся пиксельной и выравнивается при любом масштабе меню.
        status = std::make_shared<UI::Label>(u8"Esc: назад / пауза. Колесо: прокрутка / gui_demo_settings.tmp");
        UI::Placement placement;
        placement.size = Size2{0, 34};
        placement.horizontal = UI::Align::Stretch;
        placement.vertical = UI::Align::End;
        placement.scale = UI::ScaleMode::Pixels;
        status->setPlacement(placement);
        status->style().debugPixel = 1;
        // Общий статус рисуется отдельным контекстом и не перехватывает ввод.
        auto footer = std::make_shared<UI::Screen>();
        footer->add(status);
        footerGui.setScreen(footer);
        applyStyle();
    }

    void changeScale(float delta)
    {
        draft.scale = std::clamp(draft.scale + delta, 0.75F, 2.0F);
        scaleValue->setValue(draft.scale, 2);
    }
    void openSettings()
    {
        draft = applied;
        vsync->setChecked(draft.vsync);
        images->setChecked(draft.images);
        font->setChecked(draft.bitmapText);
        textFiltering->setChecked(draft.textFiltering);
        scaleValue->setValue(draft.scale, 2);
        applyStyle();
        gui.setScreen(settings);
    }
    void closeSettings()
    {
        draft = applied;
        applyStyle();
        gui.setScreen(playing ? game : mainMenu);
        if (playing) { gui.setModal(pause); }
    }
    void applySettings()
    {
        Platform::Error failure;
        if (draft.vsync != applied.vsync && !setVSync(draft.vsync, failure)) {
            message(failure.message);
            return;
        }
        std::string error;
        if (!GuiDemo::saveSettings(settingsPath, draft, error)) {
            Platform::Error rollback;
            if (!setVSync(applied.vsync, rollback)) { error += " / " + rollback.message; }
            message(error);
            return;
        }
        applied = draft;
        message(u8"Настройки сохранены / gui_demo_settings.tmp");
        closeSettings();
    }
    void pauseGame()
    {
        setPaused(true);
        gui.setModal(pause);
    }
    void applyStyle()
    {
        textFiltering->setEnabled(draft.bitmapText && bitmap.isValid());
        for (const auto& element : styled) {
            auto& style = element->style();
            style.font = draft.bitmapText ? bitmap : BitmapFont{};
            style.textFilter = draft.textFiltering ? TextureFilter::Linear : TextureFilter::Nearest;
            style.images = {};
            style.checkedImages = {};
            style.foreground = Color{0.93F, 0.96F, 1, 1};
            if (!draft.images) { continue; }
            const bool button = static_cast<bool>(std::dynamic_pointer_cast<UI::Button>(element));
            const bool check = static_cast<bool>(std::dynamic_pointer_cast<UI::Checkbox>(element));
            if (!button && !check) { continue; }
            const std::string prefix = check ? "check_box_off_"
                : element->placement().size.width < 100 ? "btn2_" : "btn1_";
            const std::array<std::string, 3> states{"normal", "hover", "pressed"};
            for (std::size_t index = 0; index < states.size(); ++index) {
                atlas.createSprite(prefix + states[index], style.images[index]);
                if (button) { style.images[index].filter = TextureFilter::Nearest; }
                if (check) { atlas.createSprite("check_box_on_" + states[index], style.checkedImages[index]); }
            }
            if (button) { style.foreground = Color{0.07F, 0.12F, 0.2F, 1}; }
        }
    }

    bool configure(const FrameInfo& frame)
    {
        const Viewport viewport{0, 0, frame.window.width, frame.window.height};
        for (std::size_t index = 0; index < panels.size(); ++index) {
            auto placement = panels[index]->placement();
            placement.size.width = std::min(panelSizes[index].width, std::max(0.0F,
                static_cast<float>(frame.window.width - 16) / draft.scale));
            placement.size.height = std::min(panelSizes[index].height, std::max(0.0F,
                static_cast<float>(frame.window.height - 84) / draft.scale));
            panels[index]->setPlacement(placement);
        }
        return gui.setViewport(viewport, draft.scale) && footerGui.setViewport(viewport);
    }
    bool onInput(const FrameInfo& frame, Platform::Error&) override
    {
        configure(frame);
        for (const auto& event : Platform::getInput().events()) {
            if (gui.handle(event)) { configure(frame); continue; }
            if (event.type == NeuexpEngine::EventType::KeyPressed && event.key == Key::Escape && !event.repeat) {
                if (gui.screen() == settings) { closeSettings(); }
                else if (playing && gui.modal()) { gui.setModal({}); setPaused(false); }
                else if (playing) { pauseGame(); }
                else { requestExit(); }
            }
        }
        return true;
    }
    bool onUpdate(const FrameInfo& frame, Platform::Error&) override
    {
        if (playing) {
            elapsed += frame.deltaTime;
            counter->setValue(elapsed, 1);
            progress->setValue(std::fmod(elapsed, 30.0F) / 30);
        }
        return true;
    }
    bool onRender(const FrameInfo& frame, Platform::Error& error) override
    {
        if (!configure(frame)) { return true; }
        beginFrame(frame.window.width, frame.window.height, Color{0.025F, 0.045F, 0.075F, 1});
        auto& renderer = getRenderer();
        UiView view;
        UiError viewError;
        UiCanvas{}.createView(Viewport{0, 0, frame.window.width, frame.window.height}, view, viewError);
        renderer.setUiView(view);
        if (playing) {
            const Vector2 center{view.size.width / 2, view.size.height * 0.63F};
            if (showOrbit) {
                renderer.drawCircle(Circle{center, 110}, Color{0.15F, 0.3F, 0.4F, 1}, ShapeFill::Outline);
            }
            renderer.drawCircle(Circle{center + Vector2{110 * std::cos(elapsed), 110 * std::sin(elapsed)}, 12},
                Color{0.25F, 0.8F, 0.65F, 1});
        }
        ResourceError failure;
        const bool drawn = gui.draw(renderer, failure) && footerGui.draw(renderer, failure);
        const bool ended = endFrame(error);
        if (!drawn) { error = Platform::Error{"GUI demo", failure.message, {}}; }
        return drawn && ended;
    }

    UI::Context gui;
    UI::Context footerGui;
    std::shared_ptr<UI::Screen> mainMenu;
    std::shared_ptr<UI::Screen> settings;
    std::shared_ptr<UI::Screen> game;
    std::shared_ptr<UI::Screen> pause;
    std::shared_ptr<UI::Label> status;
    std::shared_ptr<UI::NumericIndicator> scaleValue;
    std::shared_ptr<UI::NumericIndicator> counter;
    std::shared_ptr<UI::ProgressBar> progress;
    std::shared_ptr<UI::Checkbox> vsync;
    std::shared_ptr<UI::Checkbox> images;
    std::shared_ptr<UI::Checkbox> font;
    std::shared_ptr<UI::Checkbox> textFiltering;
    std::vector<std::shared_ptr<UI::Element>> styled;
    std::vector<std::shared_ptr<UI::Container>> panels;
    std::vector<Size2> panelSizes;
    std::vector<UI::Subscription> subscriptions;
    TextureAtlas atlas;
    BitmapFont bitmap;
    GuiDemo::Settings applied;
    GuiDemo::Settings draft;
    std::filesystem::path settingsPath;
    float elapsed = 0;
    bool playing = false;
    bool imagesAvailable = false;
    bool showOrbit = true;
};
}

int NeuexpEngine::applicationMain()
{
    MenuDemo app;
    ApplicationOptions options;
    options.window.title = "GUI / Orbital menu";
    options.window.width = 900;
    options.window.height = 700;
    options.window.minimumWidth = 480;
    options.window.minimumHeight = 360;
    Platform::Error error;
    if (!app.run(options, error)) {
        Platform::reportError(error);
        return 1;
    }
    return 0;
}
