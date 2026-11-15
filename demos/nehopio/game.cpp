#include "game.h"

#include "assets.h"
#include "renderer.h"
#include "ui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace Nehopio
{
namespace
{
constexpr float GroundRowOffset = 2;

constexpr Engine::Color TextColor{0.97F, 0.98F, 1.0F};

// Фон прокручивается с параллаксом; шаги и запас за краем экрана задают частоту рядов.
constexpr float HillSpacing = 520;
constexpr float HillMargin = 200;
constexpr float HillParallax = 0.5F;
constexpr float CloudSpacing = 430;
constexpr float CloudMargin = 160;
constexpr float CloudParallax = 0.7F;
constexpr float CloudTop = 24;
constexpr float CloudRowSpacing = 38;
// Узор провала уходит в глубину вместе с холмами: прокручивается медленнее земли.
constexpr float AbyssParallax = 0.5F;

// Оверлеи состояний: отступ заголовка от центра экрана и затенение сцены.
constexpr float WinTitleOffset = 120;
constexpr Engine::Color WinDim{0.02F, 0.04F, 0.08F, 0.5F};
constexpr Engine::Color PauseDim{0.02F, 0.04F, 0.08F, 0.45F};

float clampCenter(float value, float viewSize, float worldSize)
{
    if (worldSize <= viewSize) {
        return worldSize / 2;
    }
    return std::clamp(value, viewSize / 2, worldSize - viewSize / 2);
}

Engine::DrawOptions textOptions()
{
    Engine::DrawOptions options;
    options.color = TextColor;
    return options;
}
}

bool Game::load(const Assets& assets, const Engine::ResourceFiles& files, Engine::ResourceError& error)
{
    if (!run.load(files, assets, error)) {
        return false;
    }
    // Спрайты и шрифт приходят готовыми: отрисовка меняет только их положение.
    hillSprite = assets.hillSprite;
    bushSprite = assets.bushSprite;
    flagSprite = assets.flagSprite;
    tileSprites = assets.tileSprites;
    cloudSprites = assets.cloudSprites;
    hudFont = assets.font;
    // Кусты ставятся вдоль уровня там, где под ними сплошная земля.
    const Level& level = run.level();
    const float groundTop = level.pixelHeight() - GroundRowOffset * Level::TileSize;
    for (float x = 40; x + 96 < level.pixelWidth(); x += 360) {
        if (level.solidAt(Engine::Vector2{x + 8, groundTop + 8})
            && level.solidAt(Engine::Vector2{x + 88, groundTop + 8})
            && !level.solidAt(Engine::Vector2{x + 48, groundTop - 8})) {
            bushPositions.push_back(Engine::Vector2{x, groundTop - bushSprite.size.height});
        }
    }
    if (!hudFont.layout(u8"A/D — бег, Space — прыжок, P — пауза, R — заново, Esc — выход", texts.hint, error)
        || !hudFont.layout(u8"УРОВЕНЬ ПРОЙДЕН!", texts.winTitle, error)
        || !hudFont.layout(u8"ПАУЗА", texts.pause, error)) {
        return false;
    }
    return updateTexts(error);
}

void Game::restart()
{
    run.reset();
    cameraInitialized = false;
    hudText.clear();
    winNoteText.clear();
}

bool Game::update(float deltaTime, const Controls& controls, Engine::ResourceError& error)
{
    run.update(deltaTime, controls);
    updateCamera(deltaTime);
    return updateTexts(error);
}

void Game::updateCamera(float deltaTime)
{
    const Player& player = run.player();
    const float lookahead = std::clamp(player.horizontalVelocity() * 0.12F, -36.0F, 36.0F);
    const Engine::Vector2 desired{player.position().x + Player::Width / 2 + lookahead,
        player.position().y + Player::Height / 2 - 30};
    if (!cameraInitialized) {
        cameraCenter = desired;
        cameraInitialized = true;
        return;
    }
    const float horizontal = 1 - std::exp(-12.0F * deltaTime);
    const float vertical = 1 - std::exp(-7.0F * deltaTime);
    cameraCenter.x += (desired.x - cameraCenter.x) * horizontal;
    cameraCenter.y += (desired.y - cameraCenter.y) * vertical;
}

bool Game::updateTexts(Engine::ResourceError& error)
{
    const std::string next = u8"Монеты: " + std::to_string(run.collectedCoins()) + "/"
        + std::to_string(run.totalCoins()) + u8"   Жизни: " + std::to_string(run.lives())
        + u8"   Счёт: " + std::to_string(run.score());
    if (next != hudText) {
        if (!hudFont.layout(next, texts.hud, error)) {
            return false;
        }
        hudText = next;
    }
    if (run.phase() != Phase::Won) {
        return true;
    }
    // Подпись победы содержит итог, поэтому раскладка обновляется только при его изменении.
    const std::string note = u8"Монеты: " + std::to_string(run.collectedCoins()) + "/"
        + std::to_string(run.totalCoins()) + u8"   Счёт: " + std::to_string(run.score())
        + u8"   R — ещё раз, Esc — выход";
    if (note != winNoteText) {
        if (!hudFont.layout(note, texts.winNote, error)) {
            return false;
        }
        winNoteText = note;
    }
    return true;
}

const std::vector<Sound>& Game::sounds() const
{
    return run.sounds();
}

bool Game::prepareView(int width, int height, Engine::CameraError& error)
{
    frameWidth = width;
    frameHeight = height;
    // Спрайты масштабируются вместе с высотой окна; базовый размер — при высоте 720 пикселей.
    constexpr float BaseZoom = 2.0F;
    constexpr float BaseHeight = 720.0F;
    const float zoom = BaseZoom * static_cast<float>(height) / BaseHeight;
    const float viewWidth = static_cast<float>(width) / zoom;
    const float viewHeight = static_cast<float>(height) / zoom;
    const Level& level = run.level();
    Engine::Camera2 camera;
    camera.zoom = zoom;
    camera.referenceSize = Engine::Size2{static_cast<float>(width), static_cast<float>(height)};
    camera.mode = Engine::ViewportMode::Expand;
    camera.position = Engine::Vector2{clampCenter(cameraCenter.x, viewWidth, level.pixelWidth()),
        clampCenter(cameraCenter.y, viewHeight, level.pixelHeight())};
    return camera.createView(Engine::Viewport{0, 0, width, height}, view, error);
}

bool Game::draw(Engine::Renderer2D& renderer, const Assets& assets, bool paused, Engine::ResourceError& error)
{
    renderer.setWorldView(view);
    drawBackground(renderer, assets);
    drawTiles(renderer);
    drawEntities(renderer);
    drawPlayer(renderer);
    return drawHud(renderer, assets, paused, error);
}

void Game::drawBackground(Engine::Renderer2D& renderer, const Assets& assets)
{
    const Level& level = run.level();
    const Engine::Rect visible = view.worldBounds;
    const float shift = visible.position.x;
    const float groundTop = level.pixelHeight() - GroundRowOffset * Level::TileSize;
    const float visibleBottom = visible.position.y + visible.size.height;
    const float skyBottom = std::min(visibleBottom, groundTop);
    if (skyBottom > visible.position.y) {
        const Engine::Rect skyArea{visible.position.x, visible.position.y,
            visible.size.width, skyBottom - visible.position.y};
        renderer.drawTexture(assets.sky, skyArea);
    }

    const float hillOffset = shift * HillParallax;
    const int firstHill = static_cast<int>(
        std::floor((visible.position.x - hillOffset - HillMargin) / HillSpacing));
    const int lastHill = static_cast<int>(
        std::ceil((visible.position.x + visible.size.width - hillOffset + HillMargin) / HillSpacing));
    for (int index = firstHill; index <= lastHill; ++index) {
        // Холмы — фон с параллаксом: не привязаны к земле и не пропадают над оврагами.
        hillSprite.transform.position = Engine::Vector2{hillOffset + static_cast<float>(index) * HillSpacing,
            groundTop - hillSprite.size.height};
        renderer.drawSprite(hillSprite);
    }

    const float cloudOffset = shift * CloudParallax;
    const int firstCloud = static_cast<int>(
        std::floor((visible.position.x - cloudOffset - CloudMargin) / CloudSpacing));
    const int lastCloud = static_cast<int>(
        std::ceil((visible.position.x + visible.size.width - cloudOffset + CloudMargin) / CloudSpacing));
    for (int index = firstCloud; index <= lastCloud; ++index) {
        Engine::Sprite& cloud = cloudSprites[static_cast<std::size_t>(((index % 2) + 2) % 2)];
        cloud.transform.position = Engine::Vector2{cloudOffset + static_cast<float>(index) * CloudSpacing,
            CloudTop + static_cast<float>(((index % 3) + 3) % 3) * CloudRowSpacing};
        renderer.drawSprite(cloud);
    }

    for (const auto& position : bushPositions) {
        if (position.x + bushSprite.size.width < visible.position.x
            || position.x > visible.position.x + visible.size.width) {
            continue;
        }
        bushSprite.transform.position = position;
        renderer.drawSprite(bushSprite);
    }

    // Провалы закрывают фон, но остаются под плитками земли.
    if (visibleBottom > groundTop) {
        // Узор провала сдвигается медленнее земли: в глубине он читается как дальний план.
        const float abyssShift = std::fmod(shift * AbyssParallax, Level::TileSize);
        const float pitLeft = visible.position.x - abyssShift;
        const Engine::Rect pitArea{pitLeft, groundTop, visible.position.x + visible.size.width - pitLeft,
            visibleBottom - groundTop};
        Engine::TextureSampling pitSampling;
        pitSampling.filter = Engine::TextureFilter::Nearest;
        pitSampling.wrapX = Engine::TextureWrap::Repeat;
        pitSampling.wrapY = Engine::TextureWrap::Repeat;
        pitSampling.repeats = Engine::Vector2{pitArea.size.width / Level::TileSize,
            pitArea.size.height / Level::TileSize};
        renderer.drawTexture(assets.abyss, pitArea, Engine::DrawOptions{}, pitSampling);
    }
}

void Game::drawTiles(Engine::Renderer2D& renderer)
{
    const Level& level = run.level();
    const TileRange range = level.tileRange(view.worldBounds);
    for (int row = range.firstRow; row <= range.lastRow; ++row) {
        for (int column = range.firstColumn; column <= range.lastColumn; ++column) {
            const auto found = tileSprites.find(level.tile(column, row));
            if (found == tileSprites.end()) {
                continue;
            }
            found->second.transform.position = Engine::Vector2{static_cast<float>(column) * Level::TileSize,
                static_cast<float>(row) * Level::TileSize};
            renderer.drawSprite(found->second);
        }
    }
}

void Game::drawEntities(Engine::Renderer2D& renderer)
{
    const Engine::Rect goal = run.level().goal();
    flagSprite.transform.position = Engine::Vector2{
        goal.position.x + goal.size.width / 2 - flagSprite.size.width / 2,
        goal.position.y + goal.size.height - flagSprite.size.height};
    renderer.drawSprite(flagSprite);

    // Спрайты объектов уже содержат кадр, положение и оформление.
    for (const auto& coin : run.coins()) {
        if (!coin.collected) {
            renderer.drawSprite(coin.spin);
        }
    }
    for (const auto& item : run.items()) {
        renderer.drawSprite(item.sprite);
    }
    for (const auto& enemy : run.enemies()) {
        renderer.drawSprite(enemy.animations.current());
    }
    for (const auto& popup : run.popups()) {
        renderer.drawSprite(popup.animation);
    }
}

void Game::drawPlayer(Engine::Renderer2D& renderer) const
{
    Engine::Sprite sprite = run.player().sprite();
    // Урон не останавливает игру: персонаж мигает на месте, пока неуязвим.
    sprite.visible = run.player().blinkVisible();
    renderer.drawSprite(sprite);
}

void Game::drawPaneledText(Engine::Renderer2D& renderer, const Assets& assets, Engine::Size2 uiSize,
    const Engine::TextLayout& text, float top, Engine::Size2 padding) const
{
    const Engine::Size2 size = text.size();
    const float left = (uiSize.width - size.width) / 2;
    const Engine::Rect panelArea{left - padding.width, top - padding.height,
        size.width + 2 * padding.width, size.height + 2 * padding.height};
    renderer.drawTexture(assets.panel, panelArea);
    renderer.drawText(text, Engine::Vector2{left, top}, textOptions(),
        Engine::TextureFilter::Nearest);
}

void Game::drawBanner(Engine::Renderer2D& renderer, const Assets& assets, Engine::Size2 uiSize,
    const Engine::TextLayout& title, const Engine::TextLayout& note, float top, Engine::Color dim) const
{
    renderer.drawRectangle(Engine::Rect{0, 0, uiSize.width, uiSize.height}, dim);
    drawPaneledText(renderer, assets, uiSize, title, top, Engine::Size2{24, 16});
    drawPaneledText(renderer, assets, uiSize, note, top + title.size().height + 30, Engine::Size2{16, 8});
}

void Game::drawCenteredText(Engine::Renderer2D& renderer, Engine::Size2 uiSize, const Engine::TextLayout& text,
    Engine::Color dim) const
{
    renderer.drawRectangle(Engine::Rect{0, 0, uiSize.width, uiSize.height}, dim);
    const Engine::Size2 size = text.size();
    renderer.drawText(text, Engine::Vector2{(uiSize.width - size.width) / 2, (uiSize.height - size.height) / 2},
        textOptions(), Engine::TextureFilter::Nearest);
}

bool Game::drawHud(Engine::Renderer2D& renderer, const Assets& assets, bool paused,
    Engine::ResourceError& error) const
{
    Engine::UiCanvas canvas;
    canvas.scale = 1.0F;
    Engine::UiView ui;
    Engine::UiError uiError;
    if (!canvas.createView(Engine::Viewport{0, 0, frameWidth, frameHeight}, ui, uiError)) {
        error = {Engine::ResourceErrorCode::Graphics, "Cannot calculate HUD viewport."};
        return false;
    }
    renderer.setUiView(ui);

    // Панель подписей: внешний отступ, внутренние поля и зазор между строками.
    const Engine::Size2 hudSize = texts.hud.size();
    const Engine::Size2 hintSize = texts.hint.size();
    const Engine::Rect hudPanel{8, 6, std::max(hudSize.width, hintSize.width) + 24,
        hudSize.height + hintSize.height + 18};
    renderer.drawTexture(assets.panel, hudPanel);
    renderer.drawText(texts.hud, Engine::Vector2{16, 12}, textOptions(), Engine::TextureFilter::Nearest);
    renderer.drawText(texts.hint, Engine::Vector2{16, 12 + hudSize.height + 4}, textOptions(),
        Engine::TextureFilter::Nearest);

    if (run.phase() == Phase::Won) {
        drawBanner(renderer, assets, ui.size, texts.winTitle, texts.winNote,
            ui.size.height / 2 - WinTitleOffset, WinDim);
    }
    if (paused && run.phase() == Phase::Playing) {
        drawCenteredText(renderer, ui.size, texts.pause, PauseDim);
    }
    return true;
}
}
