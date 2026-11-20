#pragma once

#include "core/camera.h"
#include "graphics/font.h"
#include "core/resources.h"
#include "run.h"

#include <array>
#include <map>
#include <string>
#include <vector>

namespace NeuexpEngine
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
    bool load(const Assets& assets, const NeuexpEngine::ResourceFiles& files, NeuexpEngine::ResourceError& error);
    void restart();
    bool update(float deltaTime, const Controls& controls, NeuexpEngine::ResourceError& error);
    bool prepareView(int width, int height, NeuexpEngine::CameraError& error);
    bool draw(NeuexpEngine::Renderer2D& renderer, const Assets& assets, bool paused, NeuexpEngine::ResourceError& error);
    const std::vector<Sound>& sounds() const;

private:
    bool updateTexts(NeuexpEngine::ResourceError& error);
    void updateCamera(float deltaTime);
    void drawBackground(NeuexpEngine::Renderer2D& renderer, const Assets& assets);
    void drawTiles(NeuexpEngine::Renderer2D& renderer);
    void drawEntities(NeuexpEngine::Renderer2D& renderer);
    void drawPlayer(NeuexpEngine::Renderer2D& renderer) const;
    // Подпись на панели, выровненная по центру окна; панель повторяет размер текста.
    void drawPaneledText(NeuexpEngine::Renderer2D& renderer, const Assets& assets, NeuexpEngine::Size2 uiSize,
        const NeuexpEngine::TextLayout& text, float top, NeuexpEngine::Size2 padding) const;
    void drawBanner(NeuexpEngine::Renderer2D& renderer, const Assets& assets, NeuexpEngine::Size2 uiSize,
        const NeuexpEngine::TextLayout& title, const NeuexpEngine::TextLayout& note, float top, NeuexpEngine::Color dim) const;
    void drawCenteredText(NeuexpEngine::Renderer2D& renderer, NeuexpEngine::Size2 uiSize, const NeuexpEngine::TextLayout& text,
        NeuexpEngine::Color dim) const;
    bool drawHud(NeuexpEngine::Renderer2D& renderer, const Assets& assets, bool paused,
        NeuexpEngine::ResourceError& error) const;

    Run run;
    // Спрайты фона и плиток готовятся при загрузке: отрисовка меняет только их положение.
    NeuexpEngine::Sprite hillSprite;
    NeuexpEngine::Sprite bushSprite;
    NeuexpEngine::Sprite flagSprite;
    // Спрайты плиток по символу карты: набор задаёт tiles.h.
    std::map<char, NeuexpEngine::Sprite> tileSprites;
    std::array<NeuexpEngine::Sprite, 2> cloudSprites;
    NeuexpEngine::CameraView view;
    NeuexpEngine::BitmapFont hudFont;
    // Раскладки подписей собираются при загрузке и обновляются только при смене текста.
    struct Texts
    {
        NeuexpEngine::TextLayout hud;
        NeuexpEngine::TextLayout hint;
        NeuexpEngine::TextLayout winTitle;
        NeuexpEngine::TextLayout winNote;
        NeuexpEngine::TextLayout pause;
    };
    Texts texts;
    std::string hudText;
    std::string winNoteText;
    // Кусты ставятся один раз по данным уровня, чтобы не зависеть от камеры.
    std::vector<NeuexpEngine::Vector2> bushPositions;
    NeuexpEngine::Vector2 cameraCenter;
    bool cameraInitialized = false;
    int frameWidth = 1;
    int frameHeight = 1;
};
}
