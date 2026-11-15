#include "run.h"

#include "geometry.h"
#include "assets.h"
#include "tiles.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Nehopio
{
namespace
{
constexpr float FixedStep = 1.0F / 120.0F;
constexpr float EnemySpeed = 55;
constexpr float FlyerPatrolRadius = 40;
// Подлёт раздавленного врага: начальная скорость вверх, ускорение падения и
// расстояние ниже уровня, после которого враг удаляется.
constexpr float FallLaunchSpeed = -180;
constexpr float FallGravity = 2100;
constexpr float FallLimit = 96;
// Тело гриба меньше его картинки на два пикселя с каждой стороны: так он проходит в проёмы.
constexpr NeuexpEngine::Size2 MushroomBox{28, 28};
// Тело сдвинуто внутрь картинки: видимая часть гриба стоит ровно на границе плитки.
constexpr float MushroomInset = 2;
constexpr float PopupLifetime = 0.5F;
constexpr float PopupRiseSpeed = 140;
}

bool Run::load(const NeuexpEngine::ResourceFiles& files, const Assets& source, NeuexpEngine::ResourceError& error)
{
    // Шаблоны кадров копируются из ресурсов: перезапуск даёт объектам их копии.
    coinTemplate = source.coin;
    popupTemplate = source.popup;
    walkerAnimations = source.walker;
    flyerAnimations = source.flyer;
    mushroomSprite = source.mushroomSprite;
    playerValue.load(source);
    if (!levelValue.load(files, "nehopio/level.txt", error)) {
        return false;
    }
    lifeCount = InitialLives;
    restart();
    return true;
}

void Run::restart()
{
    // Перезапуск возвращает и уровень: сломанные блоки и использованные клетки восстанавливаются.
    levelValue.restore();
    playerValue.reset(levelValue.playerSpawn());
    coinList.clear();
    for (const auto& spawn : levelValue.coinSpawns()) {
        Coin coin;
        coin.spin = coinTemplate;
        coin.spin.transform.position = NeuexpEngine::Vector2{spawn.x - coin.spin.size.width / 2,
            spawn.y - coin.spin.size.height / 2};
        coinList.push_back(coin);
    }
    enemyList.clear();
    for (const auto& spawn : levelValue.enemySpawns()) {
        Enemy enemy;
        enemy.animations = walkerAnimations;
        enemy.position = NeuexpEngine::Vector2{spawn.x - EnemyWidth / 2, spawn.y - EnemyHeight};
        enemyList.push_back(enemy);
    }
    for (const auto& spawn : levelValue.flyerSpawns()) {
        Enemy enemy;
        enemy.kind = EnemyKind::Flyer;
        enemy.animations = flyerAnimations;
        enemy.baseY = spawn.y;
        enemy.baseX = spawn.x;
        enemy.position = NeuexpEngine::Vector2{spawn.x - EnemyWidth / 2, spawn.y - EnemyHeight / 2};
        enemyList.push_back(enemy);
    }
    coinTotal = static_cast<int>(coinList.size());
    popupList.clear();
    itemList.clear();
    soundList.clear();
    collected = 0;
    scoreValue = 0;
    accumulator = 0;
    jumpQueued = false;
    deathBlink = false;
    state = Phase::Playing;
}

void Run::reset()
{
    lifeCount = InitialLives;
    restart();
}

void Run::respawn()
{
    // Уровень и прогресс остаются прежними: сбрасывается только игрок.
    playerValue.reset(levelValue.playerSpawn());
    // Маленьким персонаж бывает только на последней жизни: с запасом жизней
    // после гибели он снова в большой форме.
    if (lifeCount > 1) {
        playerValue.grow();
    }
    accumulator = 0;
    jumpQueued = false;
    state = Phase::Playing;
}

// Смерть: потеря жизни списывает её и звучит гибелью, а игрок получает мигание
// и неуязвимость. На последней жизни мигание длится до перезапуска уровня.
void Run::loseLife()
{
    lifeCount = std::max(0, lifeCount - 1);
    soundList.push_back(Sound::Death);
    // На последней жизни персонаж возвращается к обычной форме.
    if (lifeCount == 1) {
        playerValue.shrink();
    }
    deathBlink = lifeCount == 0;
}

// Удар врагом: персонаж теряет жизнь и мигает на месте, игра продолжается.
// Пока идёт мигание после последней жизни, персонаж неуязвим: ни урона, ни звука удара.
void Run::damage()
{
    if (deathBlink) {
        return;
    }
    loseLife();
    playerValue.hurt();
}

// Падение в яму: жизни остались — персонаж снова на точке спавна; на последней жизни
// он мигает там, где упал, и уровень перезапустится после мигания.
void Run::fallIntoPit()
{
    loseLife();
    if (lifeCount > 0) {
        respawn();
    }
    playerValue.hurt();
}

void Run::update(float deltaTime, const Controls& controls)
{
    soundList.clear();
    if (controls.jump) {
        jumpQueued = true;
    }
    if (state != Phase::Playing) {
        return;
    }
    accumulator += std::min(deltaTime, 0.1F);
    bool firstStep = true;
    while (accumulator >= FixedStep) {
        Controls current = controls;
        current.jump = jumpQueued && firstStep;
        step(FixedStep, current);
        if (current.jump) {
            jumpQueued = false;
        }
        firstStep = false;
        accumulator -= FixedStep;
        if (state != Phase::Playing) {
            accumulator = 0;
            break;
        }
    }
    // Мигание после последней жизни закончилось: уровень начинается заново.
    if (deathBlink && !playerValue.invulnerable()) {
        reset();
    }
}

void Run::step(float deltaTime, const Controls& controls)
{
    // Порядок важен: сначала игрок и плитки, затем объекты мира, и только после сбора
    // наград проверяются контакты с врагами — собранная монета не должна спасать от смерти.
    // Пока персонаж мигает после последней жизни, управление не принимается.
    const Controls applied = deathBlink ? Controls{} : controls;
    playerValue.update(deltaTime, applied, levelValue);
    if (playerValue.takeJumpStarted()) {
        soundList.push_back(Sound::Jump);
    }
    handleCeilingHit();
    for (auto& popup : popupList) {
        popup.time += deltaTime;
        popup.animation.update(deltaTime);
        popup.animation.transform.position.y -= deltaTime * PopupRiseSpeed;
        popup.animation.color.alpha = 1 - popup.time / PopupLifetime;
    }
    popupList.erase(std::remove_if(popupList.begin(), popupList.end(),
        [](const Popup& popup) { return popup.time > PopupLifetime; }), popupList.end());
    updateEnemies(deltaTime);
    updateItems(deltaTime);
    collectItems();
    updateCoins(deltaTime);
    resolveEnemyContacts();
    // Во время мигания после последней жизни персонаж ниже уровня не считается упавшим.
    if (!deathBlink && playerValue.position().y > levelValue.pixelHeight() + 48) {
        fallIntoPit();
        return;
    }
    if (NeuexpEngine::intersects(playerValue.bounds(), levelValue.goal())) {
        finishRun();
    }
}

void Run::finishRun()
{
    playerValue.win();
    soundList.push_back(Sound::Win);
    scoreValue += 500;
    state = Phase::Won;
}

void Run::updateEnemies(float deltaTime)
{
    for (auto& enemy : enemyList) {
        enemy.elapsed += deltaTime;
        if (enemy.falling) {
            // Раздавленный враг подлетает, вращается и падает за нижнюю границу уровня.
            enemy.position.y += enemy.fallSpeed * deltaTime;
            enemy.fallSpeed += FallGravity * deltaTime;
        } else {
            walkEnemy(enemy, deltaTime);
        }
        placeEnemy(enemy);
    }
    enemyList.erase(std::remove_if(enemyList.begin(), enemyList.end(),
        [this](const Enemy& enemy) {
            return enemy.falling && enemy.position.y > levelValue.pixelHeight() + FallLimit;
        }), enemyList.end());
}

void Run::walkEnemy(Enemy& enemy, float deltaTime)
{
    enemy.animations.select(enemy.direction < 0 ? "walk.left" : "walk");
    enemy.animations.update(deltaTime);
    const float nextX = enemy.position.x + enemy.direction * EnemySpeed * deltaTime;
    // Летающий враг разворачивает на границе патруля, к которой он движется: проверка
    // «вышел за пределы» разворачивала бы его каждый шаг на границе, и он замирал бы.
    const float centerX = enemy.position.x + EnemyWidth / 2;
    const bool leavingPatrol = enemy.direction > 0
        ? centerX > enemy.baseX + FlyerPatrolRadius
        : centerX < enemy.baseX - FlyerPatrolRadius;
    if (enemy.kind == EnemyKind::Flyer && leavingPatrol) {
        enemy.direction = -enemy.direction;
        return;
    }
    // Препятствие перед корпусом поворачивает любого врага, обрыв под ногами — только ходячего.
    const float sideX = nextX + (enemy.direction > 0 ? EnemyWidth : 0) + enemy.direction * 2;
    if (levelValue.solidAt(NeuexpEngine::Vector2{sideX, enemy.position.y + EnemyHeight / 2})) {
        enemy.direction = -enemy.direction;
        return;
    }
    const float footX = nextX + (enemy.direction > 0 ? EnemyWidth : 0) + enemy.direction * 4;
    if (enemy.kind == EnemyKind::Walker
        && !levelValue.solidAt(NeuexpEngine::Vector2{footX, enemy.position.y + EnemyHeight + 4})) {
        enemy.direction = -enemy.direction;
        return;
    }
    enemy.position.x = nextX;
    if (enemy.kind == EnemyKind::Flyer) {
        enemy.position.y = enemy.baseY - EnemyHeight / 2 + std::sin(enemy.elapsed * 3.2F) * 10.0F;
    }
}

void Run::placeEnemy(Enemy& enemy)
{
    // Положение и поворот хранит кадр врага, поэтому отрисовка только рисует его.
    NeuexpEngine::Animation& frame = enemy.animations.current();
    frame.transform.position = NeuexpEngine::Vector2{enemy.position.x + EnemyWidth / 2 - frame.size.width / 2,
        enemy.position.y + EnemyHeight - frame.size.height};
    frame.transform.rotation = enemy.falling ? enemy.elapsed * 7 : 0;
}

void Run::handleCeilingHit()
{
    int column = 0;
    int row = 0;
    if (!playerValue.takeCeilingHit(column, row)) {
        return;
    }
    const TileInfo& tile = tileInfo(levelValue.tile(column, row));
    if (tile.bump == TileBump::None) {
        return;
    }
    levelValue.setTile(column, row, tile.bumpInto);
    switch (tile.bump) {
    case TileBump::Coin: {
        // Монета поднимается из блока и исчезает.
        Popup popup;
        popup.animation = popupTemplate;
        popup.animation.transform.position = NeuexpEngine::Vector2{
            static_cast<float>(column) * Level::TileSize + Level::TileSize / 2 - popup.animation.size.width / 2,
            static_cast<float>(row) * Level::TileSize - popup.animation.size.height / 2};
        popupList.push_back(popup);
        soundList.push_back(Sound::Coin);
        scoreValue += 100;
        break;
    }
    case TileBump::Break:
        soundList.push_back(Sound::Break);
        scoreValue += 50;
        break;
    case TileBump::Mushroom: {
        // Гриб выезжает из блока и дальше живёт как предмет.
        Item item;
        item.sprite = mushroomSprite;
        item.position = NeuexpEngine::Vector2{static_cast<float>(column) * Level::TileSize + 2,
            static_cast<float>(row) * Level::TileSize - MushroomBox.height};
        placeItem(item);
        item.velocity = NeuexpEngine::Vector2{70, -150};
        itemList.push_back(item);
        soundList.push_back(Sound::Bump);
        break;
    }
    case TileBump::None:
        break;
    }
}

bool Run::itemBlocked(NeuexpEngine::Vector2 position) const
{
    return levelValue.overlapsSolid(NeuexpEngine::Rect{position, MushroomBox});
}

void Run::updateItems(float deltaTime)
{
    for (auto& item : itemList) {
        if (item.collected) {
            continue;
        }
        NeuexpEngine::Vector2& position = item.position;
        item.velocity.y = std::min(item.velocity.y + FallGravity * deltaTime, 900.0F);
        const float moveX = item.velocity.x * deltaTime;
        if (moveX != 0) {
            const int steps = std::max(1, static_cast<int>(std::ceil(std::abs(moveX))));
            const float step = moveX / static_cast<float>(steps);
            for (int index = 0; index < steps; ++index) {
                if (itemBlocked(position + NeuexpEngine::Vector2{step, 0})) {
                    item.velocity.x = -item.velocity.x;
                    break;
                }
                position.x += step;
            }
        }
        const float moveY = item.velocity.y * deltaTime;
        if (moveY != 0) {
            const int steps = std::max(1, static_cast<int>(std::ceil(std::abs(moveY))));
            const float step = moveY / static_cast<float>(steps);
            for (int index = 0; index < steps; ++index) {
                if (itemBlocked(position + NeuexpEngine::Vector2{0, step})) {
                    item.velocity.y = 0;
                    break;
                }
                position.y += step;
            }
        }
        if (position.y > levelValue.pixelHeight() + 96) {
            item.collected = true;
        }
        placeItem(item);
    }
    itemList.erase(std::remove_if(itemList.begin(), itemList.end(),
        [](const Item& item) { return item.collected; }), itemList.end());
}

void Run::placeItem(Item& item)
{
    // Видимая часть гриба стоит на границе плитки: кадр сдвинут внутрь на поля картинки.
    item.sprite.transform.position = NeuexpEngine::Vector2{item.position.x - MushroomInset, item.position.y - MushroomInset};
}

void Run::collectItems()
{
    for (auto& item : itemList) {
        if (item.collected) {
            continue;
        }
        const NeuexpEngine::Rect box{item.position, MushroomBox};
        if (!NeuexpEngine::intersects(playerValue.bounds(), box)) {
            continue;
        }
        item.collected = true;
        playerValue.grow();
        ++lifeCount;
        scoreValue += 1000;
        soundList.push_back(Sound::PowerUp);
    }
}

void Run::updateCoins(float deltaTime)
{
    for (auto& coin : coinList) {
        if (coin.collected) {
            continue;
        }
        coin.spin.update(deltaTime);
        const NeuexpEngine::Rect area{coin.spin.transform.position, coin.spin.size};
        if (NeuexpEngine::intersects(playerValue.bounds(), area)) {
            coin.collected = true;
            soundList.push_back(Sound::Coin);
            ++collected;
            scoreValue += 100;
        }
    }
}

void Run::resolveEnemyContacts()
{
    const NeuexpEngine::Rect playerBox = playerValue.bounds();
    // Решение об ударе сверху принимается один раз на шаг: отскок от первого врага
    // меняет вертикальную скорость, и следующий враг под ногами уже не считался бы раздавленным.
    const bool landsOnEnemies = playerValue.verticalVelocity() > 0;
    const float playerCenter = playerBox.position.y + playerBox.size.height / 2;
    for (auto& enemy : enemyList) {
        if (enemy.falling) {
            continue;
        }
        const NeuexpEngine::Rect enemyBox{enemy.position, NeuexpEngine::Size2{EnemyWidth, EnemyHeight}};
        if (!NeuexpEngine::intersects(playerBox, enemyBox)) {
            continue;
        }
        // Падение сверху засчитывается, если центр игрока выше центра противника:
        // касание краем при приземлении не должно убивать.
        const float enemyCenter = enemyBox.position.y + EnemyHeight / 2;
        const bool stomp = landsOnEnemies && playerCenter < enemyCenter;
        if (stomp) {
            // Раздавленный кадр остаётся, пока враг подлетает и падает.
            enemy.falling = true;
            enemy.fallSpeed = FallLaunchSpeed;
            enemy.animations.select(enemy.direction < 0 ? "squash.left" : "squash");
            placeEnemy(enemy);
            playerValue.bounce();
            soundList.push_back(Sound::Stomp);
            scoreValue += 200;
        } else if (!playerValue.invulnerable()) {
            // Урон не останавливает игру: персонаж мигает на месте и продолжает движение.
            damage();
        }
    }
}

const Level& Run::level() const
{
    return levelValue;
}

const Player& Run::player() const
{
    return playerValue;
}

const std::vector<Coin>& Run::coins() const
{
    return coinList;
}

const std::vector<Enemy>& Run::enemies() const
{
    return enemyList;
}

const std::vector<Popup>& Run::popups() const
{
    return popupList;
}

Phase Run::phase() const
{
    return state;
}

int Run::collectedCoins() const
{
    return collected;
}

int Run::totalCoins() const
{
    return coinTotal;
}

int Run::score() const
{
    return scoreValue;
}

const std::vector<Item>& Run::items() const
{
    return itemList;
}

int Run::lives() const
{
    return lifeCount;
}

const std::vector<Sound>& Run::sounds() const
{
    return soundList;
}
}
