#include "player.h"

#include "assets.h"
#include "level.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Nehopio
{
namespace
{
constexpr float RunSpeed = 300;
constexpr float GroundAcceleration = 2600;
constexpr float AirAcceleration = 1700;
constexpr float GroundFriction = 3000;
constexpr float AirDrag = 900;
constexpr float Gravity = 2100;
constexpr float MaximumFallSpeed = 1100;
constexpr float JumpSpeed = 780;
constexpr float JumpCutFactor = 0.5F;
constexpr float CoyoteTime = 0.09F;
constexpr float JumpBufferTime = 0.12F;
constexpr float BounceSpeed = 420;

// Имена клипов состояний в порядке State, без взгляда влево и с ним.
constexpr std::array<const char*, 5> StateClips{"idle", "run", "jump", "fall", "win"};
constexpr std::array<const char*, 5> StateClipsLeft{"idle.left", "run.left", "jump.left", "fall.left", "win.left"};
constexpr std::size_t OrdinarySheet = 0;
constexpr std::size_t BigSheet = 1;

float approach(float value, float target, float maximumDelta)
{
    if (value < target) {
        return std::min(value + maximumDelta, target);
    }
    return std::max(value - maximumDelta, target);
}
}

Player::Player()
{
    reset(Engine::Vector2{});
}

void Player::load(const Assets& assets)
{
    // Кадры обеих форм уже готовы в ресурсах: игроку нужны собственные копии.
    presentations = assets.player;
    restartAnimations();
}

void Player::restartAnimations()
{
    for (Engine::AnimationSet& shown : presentations) {
        shown.restart();
    }
}

void Player::reset(Engine::Vector2 spawn)
{
    positionValue = Engine::Vector2{spawn.x - Width / 2, spawn.y - Height};
    velocity = Engine::Vector2{};
    groundContact = 0;
    jumpBuffer = 0;
    onGround = false;
    jumpCutAvailable = false;
    invulnerableTimer = 0;
    bigFlag = false;
    ceilingHit = false;
    won = false;
    direction = 1;
    currentState = State::Idle;
    restartAnimations();
}

void Player::hurt()
{
    invulnerableTimer = HurtTime;
}

void Player::win()
{
    won = true;
    velocity.x = 0;
    updatePresentation(0);
}

bool Player::takeCeilingHit(int& column, int& row)
{
    if (!ceilingHit) {
        return false;
    }
    column = ceilingColumn;
    row = ceilingRow;
    ceilingHit = false;
    return true;
}

bool Player::takeJumpStarted()
{
    const bool started = jumpStarted;
    jumpStarted = false;
    return started;
}

void Player::bounce()
{
    velocity.y = -BounceSpeed;
    onGround = false;
    jumpCutAvailable = false;
}

Engine::Size2 Player::size() const
{
    return Engine::Size2{bodyWidth(), bodyHeight()};
}

Engine::Rect Player::bounds() const
{
    return Engine::Rect{positionValue, size()};
}

float Player::bodyWidth() const
{
    return bigFlag ? BigWidth : Width;
}

float Player::bodyHeight() const
{
    return bigFlag ? BigHeight : Height;
}

bool Player::grow()
{
    if (bigFlag) {
        return false;
    }
    bigFlag = true;
    positionValue.y -= BigHeight - Height;
    return true;
}

Engine::Vector2 Player::position() const
{
    return positionValue;
}

float Player::horizontalVelocity() const
{
    return velocity.x;
}

float Player::verticalVelocity() const
{
    return velocity.y;
}

const Engine::Sprite& Player::sprite() const
{
    return presentations[bigFlag ? BigSheet : OrdinarySheet].current();
}

bool Player::invulnerable() const
{
    return invulnerableTimer > 0;
}

bool Player::blinkVisible() const
{
    if (!invulnerable()) {
        return true;
    }
    return std::fmod(invulnerableTimer, BlinkPeriod) < BlinkPeriod / 2;
}

void Player::update(float deltaTime, const Controls& controls, const Level& level)
{
    jumpStarted = false;
    invulnerableTimer = std::max(0.0F, invulnerableTimer - deltaTime);
    if (won) {
        updatePresentation(deltaTime);
        return;
    }
    const int inputDirection = static_cast<int>(controls.right) - static_cast<int>(controls.left);
    if (inputDirection != 0) {
        direction = inputDirection;
    }
    const float targetSpeed = static_cast<float>(inputDirection) * RunSpeed;
    const float acceleration = onGround ? GroundAcceleration : AirAcceleration;
    if (inputDirection != 0) {
        velocity.x = approach(velocity.x, targetSpeed, acceleration * deltaTime);
    } else {
        const float friction = onGround ? GroundFriction : AirDrag;
        velocity.x = approach(velocity.x, 0, friction * deltaTime);
    }

    if (controls.jump) {
        jumpBuffer = JumpBufferTime;
    }
    if (jumpBuffer > 0 && (onGround || groundContact > 0)) {
        velocity.y = -JumpSpeed;
        onGround = false;
        groundContact = 0;
        jumpBuffer = 0;
        jumpCutAvailable = true;
        jumpStarted = true;
    }
    if (!controls.jumpHeld && velocity.y < 0 && jumpCutAvailable) {
        velocity.y *= JumpCutFactor;
        jumpCutAvailable = false;
    }
    jumpBuffer = std::max(0.0F, jumpBuffer - deltaTime);
    groundContact = std::max(0.0F, groundContact - deltaTime);

    velocity.y = std::min(velocity.y + Gravity * deltaTime, MaximumFallSpeed);

    sweepHorizontal(velocity.x * deltaTime, level);
    const bool wasGrounded = onGround;
    onGround = false;
    sweepVertical(velocity.y * deltaTime, level);
    if (onGround) {
        jumpCutAvailable = false;
    } else if (wasGrounded && groundContact <= 0) {
        // Сход с края: короткое окно, в котором прыжок ещё считается прыжком с земли.
        groundContact = CoyoteTime;
    }

    const float rightLimit = level.pixelWidth() - bodyWidth();
    if (positionValue.x < 0) {
        positionValue.x = 0;
        velocity.x = std::max(0.0F, velocity.x);
    }
    if (positionValue.x > rightLimit) {
        positionValue.x = rightLimit;
        velocity.x = std::min(0.0F, velocity.x);
    }
    updatePresentation(deltaTime);
}

void Player::sweepHorizontal(float distance, const Level& level)
{
    if (distance == 0) {
        return;
    }
    const int steps = std::max(1, static_cast<int>(std::ceil(std::abs(distance))));
    const float step = distance / static_cast<float>(steps);
    for (int index = 0; index < steps; ++index) {
        const Engine::Rect candidate{positionValue + Engine::Vector2{step, 0}, size()};
        if (level.overlapsSolid(candidate)) {
            // Останавливаемся в последней допустимой точке: шаг не больше пикселя.
            velocity.x = 0;
            return;
        }
        positionValue.x += step;
    }
}

bool Player::footSupport(Engine::Vector2 position, const Level& level, float& left, float& right) const
{
    const int row = static_cast<int>(std::floor((position.y + bodyHeight()) / Level::TileSize));
    const int firstColumn = static_cast<int>(std::floor(position.x / Level::TileSize));
    const int lastColumn = static_cast<int>(std::floor((position.x + bodyWidth() - 0.001F) / Level::TileSize));
    bool found = false;
    for (int column = firstColumn; column <= lastColumn; ++column) {
        if (!level.solid(column, row)) {
            continue;
        }
        const float tileLeft = static_cast<float>(column) * Level::TileSize;
        const float overlapLeft = std::max(position.x, tileLeft);
        const float overlapRight = std::min(position.x + bodyWidth(), tileLeft + Level::TileSize);
        if (overlapRight <= overlapLeft) {
            continue;
        }
        left = found ? std::min(left, overlapLeft) : overlapLeft;
        right = found ? std::max(right, overlapRight) : overlapRight;
        found = true;
    }
    return found;
}

void Player::sweepVertical(float distance, const Level& level)
{
    if (distance == 0) {
        return;
    }
    const int steps = std::max(1, static_cast<int>(std::ceil(std::abs(distance))));
    const float step = distance / static_cast<float>(steps);
    for (int index = 0; index < steps; ++index) {
        const Engine::Rect candidate{positionValue + Engine::Vector2{0, step}, size()};
        if (!level.overlapsSolid(candidate)) {
            positionValue.y += step;
            continue;
        }
        if (step > 0) {
            // Поверхность под ногами определяется положением после шага: тело уже задело плитку.
            const int row = static_cast<int>(
                std::floor((candidate.position.y + candidate.size.height) / Level::TileSize));
            const float tileEdge = static_cast<float>(row) * Level::TileSize;
            float supportLeft = 0;
            float supportRight = 0;
            const bool support = footSupport(Engine::Vector2{positionValue.x, tileEdge - bodyHeight()}, level,
                supportLeft, supportRight);
            if (!support || supportRight - supportLeft < bodyWidth() / 3) {
                // Под ногами меньше трети твёрдой земли: персонаж соскальзывает с края в яму.
                const float bodyCenter = positionValue.x + bodyWidth() / 2;
                const float supportCenter = (supportLeft + supportRight) / 2;
                positionValue.x += bodyCenter > supportCenter
                    ? supportRight - positionValue.x
                    : supportLeft - positionValue.x - bodyWidth();
                positionValue.y += step;
                continue;
            }
            positionValue.y = std::max(positionValue.y, tileEdge - bodyHeight());
            onGround = true;
        } else {
            // Запоминаем плитку над головой: по ней решается, ломать блок или выпускать монету.
            // Голова бьёт плитку над своей серединой, поэтому из задетых плиток берём
            // ближайшую к центру тела, а не первую попавшуюся с краю.
            const int row = static_cast<int>(std::floor(candidate.position.y / Level::TileSize));
            const int firstColumn = static_cast<int>(std::floor(candidate.position.x / Level::TileSize));
            const int lastColumn = static_cast<int>(
                std::floor((candidate.position.x + bodyWidth() - 0.001F) / Level::TileSize));
            const float headCenter = candidate.position.x + bodyWidth() / 2;
            bool found = false;
            float nearestDistance = 0;
            for (int column = firstColumn; column <= lastColumn; ++column) {
                if (!level.solid(column, row)) {
                    continue;
                }
                const float center = (static_cast<float>(column) + 0.5F) * Level::TileSize;
                const float distance = std::abs(center - headCenter);
                if (!found || distance < nearestDistance) {
                    ceilingColumn = column;
                    ceilingRow = row;
                    nearestDistance = distance;
                    found = true;
                }
            }
            ceilingHit = found;
        }
        velocity.y = 0;
        return;
    }
}

void Player::updatePresentation(float deltaTime)
{
    if (won) {
        currentState = State::Win;
    } else if (!onGround) {
        currentState = velocity.y < 0 ? State::Jump : State::Fall;
    } else if (std::abs(velocity.x) > 12) {
        currentState = State::Run;
    } else {
        currentState = State::Idle;
    }
    Engine::AnimationSet& shown = presentations[bigFlag ? BigSheet : OrdinarySheet];
    // Смена состояния или направления взгляда начинает нужный клип заново.
    const std::size_t index = static_cast<std::size_t>(currentState);
    shown.select(direction < 0 ? StateClipsLeft[index] : StateClips[index]);
    shown.update(deltaTime);
    // Положение хранит сам кадр: он ставится по ногам тела, по центру по горизонтали.
    Engine::Animation& frame = shown.current();
    const Engine::Rect box = bounds();
    frame.transform.position = Engine::Vector2{box.position.x + box.size.width / 2 - frame.size.width / 2,
        box.position.y + box.size.height - frame.size.height};
}
}
