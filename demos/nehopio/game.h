#pragma once

#include "camera.h"
#include "font.h"
#include "resources.h"
#include "run.h"

#include <array>
#include <map>
#include <string>
#include <vector>

namespace Engine
{
class Renderer2D;
}

namespace Nehopio
{
struct Assets;

// Отображение прохождения: камера, фон, плитки, объекты и подписи интерфейса.
// Кадры анимаций объектов принадлежат Run, поэтому отрисовка только выбирает их.
class Game
{
public:
    // Кадры и шрифт берутся из загруженных ресурсов: спрайты собираются при загрузке.
    bool load(const Assets& assets, const Engine::ResourceFiles& files, Engine::ResourceError& error);
    void restart();
    bool update(float deltaTime, const Controls& controls, Engine::ResourceError& error);
    bool prepareView(int width, int height, Engine::CameraError& error);
    bool draw(Engine::Renderer2D& renderer, const Assets& assets, bool paused, Engine::ResourceError& error);
    const std::vector<Sound>& sounds() const;

private:
    bool updateTexts(Engine::ResourceError& error);
    void updateCamera(float deltaTime);
    void drawBackground(Engine::Renderer2D& renderer, const Assets& assets);
    void drawTiles(Engine::Renderer2D& renderer);
    void drawEntities(Engine::Renderer2D& renderer);
    void drawPlayer(Engine::Renderer2D& renderer) const;
    // Подпись на панели, выровненная по центру окна; панель повторяет размер текста.
    void drawPaneledText(Engine::Renderer2D& renderer, const Assets& assets, Engine::Size2 uiSize,
        const Engine::TextLayout& text, float top, Engine::Size2 padding) const;
    void drawBanner(Engine::Renderer2D& renderer, const Assets& assets, Engine::Size2 uiSize,
        const Engine::TextLayout& title, const Engine::TextLayout& note, float top, Engine::Color dim) const;
    void drawCenteredText(Engine::Renderer2D& renderer, Engine::Size2 uiSize, const Engine::TextLayout& text,
        Engine::Color dim) const;
    bool drawHud(Engine::Renderer2D& renderer, const Assets& assets, bool paused,
        Engine::ResourceError& error) const;

    Run run;
    // Спрайты фона и плиток готовятся при загрузке: отрисовка меняет только их положение.
    Engine::Sprite hillSprite;
    Engine::Sprite bushSprite;
    Engine::Sprite flagSprite;
    // Спрайты плиток по символу карты: набор задаёт tiles.h.
    std::map<char, Engine::Sprite> tileSprites;
    std::array<Engine::Sprite, 2> cloudSprites;
    Engine::CameraView view;
    Engine::BitmapFont hudFont;
    // Раскладки подписей собираются при загрузке и обновляются только при смене текста.
    struct Texts
    {
        Engine::TextLayout hud;
        Engine::TextLayout hint;
        Engine::TextLayout winTitle;
        Engine::TextLayout winNote;
        Engine::TextLayout pause;
    };
    Texts texts;
    std::string hudText;
    std::string winNoteText;
    // Кусты ставятся один раз по данным уровня, чтобы не зависеть от камеры.
    std::vector<Engine::Vector2> bushPositions;
    Engine::Vector2 cameraCenter;
    bool cameraInitialized = false;
    int frameWidth = 1;
    int frameHeight = 1;
};
}
