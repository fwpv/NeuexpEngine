#include "assets.h"

#include "graphics/renderer.h"
#include "tiles.h"

#include <array>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace Nehopio
{
namespace
{
// Темп кадров. Участки кадров и их имена заданы в описаниях атласов листов.
constexpr double CoinFrameTime = 0.1;
constexpr double PopupFrameTime = 0.05;
constexpr double EnemyWalkFrameTime = 0.125;
constexpr double IdleFrameTime = 0.45;
constexpr double RunFrameTime = 0.09;
// Неподвижный клип из одного кадра: длительность не влияет на показ.
constexpr double StillFrameTime = 1.0;
// Монета рисуется крупнее участка атласа; этот же размер задаёт её тело в правилах.
constexpr NeuexpEngine::Size2 CoinFrameSize{20, 20};
constexpr std::size_t OrdinarySheet = 0;
constexpr std::size_t BigSheet = 1;

// Клипы врага: походка и раздавленный кадр для обоих направлений взгляда.
const std::vector<NeuexpEngine::AnimationSet::Clip> EnemyClips{
    {"walk", {"walk.0", "walk.1"}, EnemyWalkFrameTime},
    {"walk.left", {"walk.0.left", "walk.1.left"}, EnemyWalkFrameTime},
    {"squash", {"squash"}, StillFrameTime},
    {"squash.left", {"squash.left"}, StillFrameTime},
};

// Клипы листа игрока: имя, участки атласа в порядке кадров и длительность кадра.
// Клипы взгляда влево берут кадры зеркальной половины листа.
const std::vector<NeuexpEngine::AnimationSet::Clip> PlayerClips{
    {"idle", {"idle.0", "idle.1"}, IdleFrameTime},
    {"run", {"run.0", "run.1", "run.2", "run.3"}, RunFrameTime},
    {"jump", {"jump"}, StillFrameTime},
    {"fall", {"fall"}, StillFrameTime},
    {"win", {"win"}, StillFrameTime},
    {"idle.left", {"idle.0.left", "idle.1.left"}, IdleFrameTime},
    {"run.left", {"run.0.left", "run.1.left", "run.2.left", "run.3.left"}, RunFrameTime},
    {"jump.left", {"jump.left"}, StillFrameTime},
    {"fall.left", {"fall.left"}, StillFrameTime},
    {"win.left", {"win.left"}, StillFrameTime},
};

constexpr std::array<const char*, 2> CloudNames{"cloud.0", "cloud.1"};

// Спрайт участка атласа; отсутствующее имя — ошибка описания атласа.
bool atlasSprite(const NeuexpEngine::TextureAtlas& atlas, std::string_view name, NeuexpEngine::Sprite& result,
    NeuexpEngine::ResourceError& error)
{
    if (!atlas.createSprite(name, result)) {
        error = {NeuexpEngine::ResourceErrorCode::InvalidImage, "Missing atlas region: " + std::string(name)};
        return false;
    }
    result.filter = NeuexpEngine::TextureFilter::Nearest;
    return true;
}
}

bool Assets::load(NeuexpEngine::Renderer2D& renderer, const NeuexpEngine::ResourceFiles& files, NeuexpEngine::ResourceError& error)
{
    // Заливки фона и панелей рисуются текстурами с растяжением.
    const std::array<std::pair<const char*, NeuexpEngine::Texture*>, 3> fills{{
        {"nehopio/sky.png", &sky},
        {"nehopio/abyss.png", &abyss},
        {"nehopio/panel.png", &panel},
    }};
    for (const auto& [path, target] : fills) {
        if (!renderer.loadTexture(files, path, *target, error)) {
            return false;
        }
    }
    // Листу нужны три значения — картинка, описание атласа и приёмник.
    // Текстура остаётся живой за счёт ссылки внутри атласа.
    using Sheet = std::tuple<const char*, const char*, NeuexpEngine::TextureAtlas*>;
    const std::array<Sheet, 7> sheets{{
        {"nehopio/player.png", "nehopio/player.atlas", &playerAtlas},
        {"nehopio/player_big.png", "nehopio/player_big.atlas", &playerBigAtlas},
        {"nehopio/enemy.png", "nehopio/enemy.atlas", &enemyAtlas},
        {"nehopio/flyer.png", "nehopio/flyer.atlas", &flyerAtlas},
        {"nehopio/coin.png", "nehopio/coin.atlas", &coinAtlas},
        {"nehopio/tiles.png", "nehopio/tiles.atlas", &tileAtlas},
        {"nehopio/clouds.png", "nehopio/clouds.atlas", &cloudAtlas},
    }};
    for (const auto& [image, atlas, target] : sheets) {
        NeuexpEngine::Texture texture;
        if (!renderer.loadTexture(files, image, texture, error) || !target->load(texture, files, atlas, error)) {
            return false;
        }
    }
    // Цельные картинки: спрайт берёт размер из картинки, положение задаёт отрисовка.
    const std::array<std::pair<const char*, NeuexpEngine::Sprite*>, 4> sprites{{
        {"nehopio/flag.png", &flagSprite},
        {"nehopio/hill.png", &hillSprite},
        {"nehopio/bush.png", &bushSprite},
        {"nehopio/mushroom.png", &mushroomSprite},
    }};
    for (const auto& [path, target] : sprites) {
        NeuexpEngine::Texture texture;
        if (!renderer.loadTexture(files, path, texture, error)) {
            return false;
        }
        *target = NeuexpEngine::Sprite(texture);
        target->filter = NeuexpEngine::TextureFilter::Nearest;
    }
    // Шрифт: изображение готовится как белая альфа-маска глифов.
    NeuexpEngine::Texture fontImage;
    std::string fontDescription;
    if (!renderer.loadFontImage(files, "font/fnt20.fnt", fontImage, fontDescription, error)
        || !font.load(fontImage, fontDescription, error)) {
        return false;
    }
    // Кадры монеты и всплывающей из блока монеты: один участок листа, разный темп.
    const std::initializer_list<std::string_view> coinFrames{"spin.0", "spin.1", "spin.2", "spin.3"};
    if (!coin.load(coinAtlas, coinFrames, CoinFrameTime, error)
        || !popup.load(coinAtlas, coinFrames, PopupFrameTime, error)) {
        return false;
    }
    coin.filter = NeuexpEngine::TextureFilter::Nearest;
    popup.filter = NeuexpEngine::TextureFilter::Nearest;
    coin.size = CoinFrameSize;
    popup.size = CoinFrameSize;
    coin.restart();
    popup.restart();
    // Кадры врагов: обычный ходит и падает, летающий патрулирует.
    if (!walker.load(enemyAtlas, EnemyClips, error) || !flyer.load(flyerAtlas, EnemyClips, error)) {
        return false;
    }
    walker.setFilter(NeuexpEngine::TextureFilter::Nearest);
    flyer.setFilter(NeuexpEngine::TextureFilter::Nearest);
    // Кадры персонажа: обычная и увеличенная форма, клипы у листов одинаковые.
    const std::array<const NeuexpEngine::TextureAtlas*, 2> playerSheets{&playerAtlas, &playerBigAtlas};
    for (std::size_t index = 0; index < playerSheets.size(); ++index) {
        NeuexpEngine::AnimationSet& shown = player[index];
        if (!shown.load(*playerSheets[index], PlayerClips, error)) {
            return false;
        }
        shown.setFilter(NeuexpEngine::TextureFilter::Nearest);
        shown.restart();
    }
    // Спрайты плиток по символу карты: набор задаёт tiles.h.
    for (const TileInfo& tile : tileTypes()) {
        if (!tile.region) {
            continue;
        }
        NeuexpEngine::Sprite sprite;
        if (!atlasSprite(tileAtlas, tile.region, sprite, error)) {
            return false;
        }
        tileSprites.insert_or_assign(tile.symbol, sprite);
    }
    for (std::size_t index = 0; index < cloudSprites.size(); ++index) {
        if (!atlasSprite(cloudAtlas, CloudNames[index], cloudSprites[index], error)) {
            return false;
        }
    }
    return true;
}
}
