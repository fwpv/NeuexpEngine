#pragma once

#include "graphics/animation.h"
#include "level.h"
#include "core/math2d.h"
#include "player.h"
#include "core/resources.h"
#include "sound.h"

#include <vector>

namespace Nehopio
{
struct Assets;

struct Coin
{
    // Спрайт монеты: кадр и положение для отрисовки, размер задан при перезапуске.
    NeuexpEngine::Animation spin;
    bool collected = false;
};

enum class EnemyKind
{
    Walker,
    Flyer
};

struct Enemy
{
    NeuexpEngine::Vector2 position; // Левый верхний угол корпуса.
    EnemyKind kind = EnemyKind::Walker;
    // Клипы врага: походка и раздавленный кадр для обоих направлений взгляда.
    NeuexpEngine::AnimationSet animations;
    float baseY = 0; // Средняя высота полёта для летающего врага.
    float baseX = 0; // Центр патруля по горизонтали.
    float direction = -1;
    float elapsed = 0; // Для покачивания и вращения при падении.
    float fallSpeed = 0;
    bool falling = false;
};

// Монета, выбитая из блока: поднимается и исчезает.
struct Popup
{
    // Спрайт всплывающей монеты: кадр и положение для отрисовки.
    NeuexpEngine::Animation animation;
    float time = 0;
};

enum class Phase
{
    Playing,
    Won
};

// Гриб: выезжает из блока, ходит по земле и падает в ямы.
struct Item
{
    NeuexpEngine::Vector2 position; // Левый верхний угол тела; кадр ставится со смещением полей.
    NeuexpEngine::Sprite sprite;
    NeuexpEngine::Vector2 velocity;
    bool collected = false;
};

// Прохождение уровня без отрисовки: физика игрока, противники, монеты и финиш.
// Обновление идёт фиксированным шагом, поэтому результат не зависит от FPS.
class Run
{
public:
    static constexpr float EnemyWidth = 26;
    static constexpr float EnemyHeight = 24;
    // Жизней на старте одна; грибок добавляет жизнь, обнуление перезапускает игру.
    static constexpr int InitialLives = 1;

    // Кадры анимаций копируются из загруженных ресурсов уровня.
    bool load(const NeuexpEngine::ResourceFiles& files, const Assets& source, NeuexpEngine::ResourceError& error);
    void restart();
    // Полный сброс: жизни, игрок, монеты, враги и предметы.
    void reset();
    void update(float deltaTime, const Controls& controls);

    const Level& level() const;
    const Player& player() const;
    const std::vector<Coin>& coins() const;
    const std::vector<Enemy>& enemies() const;
    const std::vector<Popup>& popups() const;
    const std::vector<Item>& items() const;
    int lives() const;
    Phase phase() const;
    int collectedCoins() const;
    int totalCoins() const;
    int score() const;
    const std::vector<Sound>& sounds() const;

private:
    // Возвращает игрока на точку спавна, сохраняя уровень и прогресс.
    void respawn();
    // Списывает жизнь и включает мигание; на последней жизни включает ожидание перезапуска.
    void loseLife();
    // Удар противника: персонаж теряет жизнь и мигает на месте, игра продолжается.
    void damage();
    // Падение в пропасть: жизнь теряется, игрок возвращается на точку спавна.
    void fallIntoPit();
    void step(float deltaTime, const Controls& controls);
    void updateEnemies(float deltaTime);
    // Правила похода: поворот у стены, у края площадки и в пределах патруля.
    void walkEnemy(Enemy& enemy, float deltaTime);
    // Переносит положение и поворот врага в его кадр для отрисовки.
    void placeEnemy(Enemy& enemy);
    void updateCoins(float deltaTime);
    void resolveEnemyContacts();
    void updateItems(float deltaTime);
    // Переносит положение тела предмета в его кадр для отрисовки.
    void placeItem(Item& item);
    void collectItems();
    bool itemBlocked(NeuexpEngine::Vector2 position) const;
    void handleCeilingHit();
    void finishRun();

    // Шаблоны кадров: объекты уровня получают их копии при каждом перезапуске.
    NeuexpEngine::Animation coinTemplate;
    NeuexpEngine::Animation popupTemplate;
    NeuexpEngine::AnimationSet walkerAnimations;
    NeuexpEngine::AnimationSet flyerAnimations;
    NeuexpEngine::Sprite mushroomSprite;
    Level levelValue;
    Player playerValue;
    std::vector<Coin> coinList;
    std::vector<Enemy> enemyList;
    std::vector<Popup> popupList;
    std::vector<Item> itemList;
    int lifeCount = InitialLives;
    std::vector<Sound> soundList;
    Phase state = Phase::Playing;
    // Нажатие прыжка ждёт ближайшего шага: при высоком FPS шаг может
    // не наступить в кадре нажатия, и событие не должно теряться.
    bool jumpQueued = false;
    float accumulator = 0;
    // Последняя жизнь потеряна: персонаж мигает без управления, а когда отмигает,
    // уровень начинается заново.
    bool deathBlink = false;
    int collected = 0;
    int coinTotal = 0;
    int scoreValue = 0;
};
}
