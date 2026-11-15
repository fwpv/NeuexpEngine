#pragma once

#include "animation.h"
#include "geometry.h"
#include "math2d.h"

#include <array>
#include <cstddef>

namespace Nehopio
{
class Level;
struct Assets;

struct Controls
{
    bool left = false;
    bool right = false;
    bool jump = false; // Нажатие в текущем кадре.
    bool jumpHeld = false;
};

// Платформенная физика на сетке плиток: ускорение, трение, прыжок с обрезкой и
// короткое окно прыжка после схода с края. Обновляется фиксированным шагом.
class Player
{
public:
    static constexpr float Width = 22;
    static constexpr float Height = 30;
    static constexpr float BigWidth = 30;
    static constexpr float BigHeight = 46;
    // Потеря жизни не снимает персонажа сразу: он остаётся на месте и мигает,
    // пока неуязвим, а уровень и прогресс сохраняются.
    static constexpr float HurtTime = 1.2F;
    static constexpr float BlinkPeriod = 0.14F;

    Player();

    // Кадры обеих форм берутся из подготовленных ресурсов.
    void load(const Assets& assets);
    void reset(NeuexpEngine::Vector2 spawn);
    void update(float deltaTime, const Controls& controls, const Level& level);
    // Урон: персонаж теряет жизнь, мигает на месте и не получает новый урон, пока мигает.
    void hurt();
    void win();
    // Игрок оттолкнулся от земли в последнем обновлении; флаг сбрасывается чтением.
    bool takeJumpStarted();
    // Отскок после удара по противнику сверху.
    void bounce();
    // Гриб: увеличивает тело и сохраняет положение ноги.
    bool grow();
    NeuexpEngine::Size2 size() const;
    // Удар головой о плитку: координаты клетки, в которую упёрлись.
    bool takeCeilingHit(int& column, int& row);

    NeuexpEngine::Rect bounds() const;
    NeuexpEngine::Vector2 position() const;
    float horizontalVelocity() const;
    float verticalVelocity() const;
    // Текущий кадр: состояние и направление взгляда задают участок листа.
    const NeuexpEngine::Sprite& sprite() const;
    // Неуязвимость после урона: контакты с врагами в это время не действуют.
    bool invulnerable() const;
    // Мигание во время неуязвимости: кадр показывается через половину периода.
    bool blinkVisible() const;

private:
    // Состояние выбирает набор кадров: покой и бег проигрываются по кругу,
    // остальные состояния показывают неподвижный кадр.
    enum class State
    {
        Idle,
        Run,
        Jump,
        Fall,
        Win
    };

    void restartAnimations();
    void sweepHorizontal(float distance, const Level& level);
    void sweepVertical(float distance, const Level& level);
    // Твёрдая опора под нижней гранью тела: промежуток по горизонтали и его наличие.
    bool footSupport(NeuexpEngine::Vector2 position, const Level& level, float& left, float& right) const;
    void updatePresentation(float deltaTime);
    float bodyWidth() const;
    float bodyHeight() const;

    // Клипы листа: обычная и увеличенная форма игрока.
    std::array<NeuexpEngine::AnimationSet, 2> presentations;
    NeuexpEngine::Vector2 positionValue;
    NeuexpEngine::Vector2 velocity;
    float groundContact = 0;
    float jumpBuffer = 0;
    bool onGround = false;
    bool jumpCutAvailable = false;
    float invulnerableTimer = 0;
    bool bigFlag = false;
    bool ceilingHit = false;
    int ceilingColumn = 0;
    int ceilingRow = 0;
    bool won = false;
    bool jumpStarted = false;
    int direction = 1;
    State currentState = State::Idle;
};
}
