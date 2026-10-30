#include "cat.h"
#include "game.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace Cat
{
namespace
{
constexpr int HeadFrames = Feet - Head;
constexpr float JumpImpulse = 810;
}

Cat::Cat()
{
    body.position = Engine::Vector2{450, 223.75F};
    body.supportShape.segments = std::array<Interval, 2>{Interval{-60.75F, -20.25F}, Interval{8.25F, 54.75F}};
    body.supportShape.overhang = 10;
    body.supportShape.heightAllowance = 5;
    body.supportShape.minimumOverlap = 3;
}

Engine::Vector2 Cat::position() const
{
    return body.position;
}

Body& Cat::physicalBody()
{
    return body;
}

void Cat::updateAnimation(float duration, const Controls& controls, bool supported)
{
    grounded = supported;
    const int direction = static_cast<int>(controls.right) - static_cast<int>(controls.left);
    constexpr float AnimationFrameDuration = 0.03F;
    animationTime += duration;
    const int steps = direction == 0 ? 0 : static_cast<int>((animationTime - frameTime) / AnimationFrameDuration);
    headFrame = std::clamp(headFrame + direction * steps, 0, HeadFrames - 1);
    constexpr int FeetFrames = ArtCount - Feet;
    feetFrame = ((feetFrame + direction * (steps % FeetFrames)) % FeetFrames + FeetFrames) % FeetFrames;
    frameTime = direction == 0 ? animationTime : frameTime + static_cast<float>(steps) * AnimationFrameDuration;
}

void Cat::draw(Engine::Renderer2D& renderer, const Images& images) const
{
    const Engine::Vector2 origin = body.position + Engine::Vector2{-101.25F, -123.75F + 3 + (grounded ? 2 : 0)};
    images.draw(renderer, Head + static_cast<std::size_t>(headFrame), origin, 0.75F);
    images.draw(renderer, Feet + static_cast<std::size_t>(feetFrame),
        origin + Engine::Vector2{44 * 0.75F, 120 * 0.75F}, 0.75F);
}

CatController::CatController(CatPhysics& physics)
    : physics(physics)
{
}

void CatController::beginFrame(const Controls& controls)
{
    direction = static_cast<int>(controls.right) - static_cast<int>(controls.left);
    if (controls.jump) {
        constexpr float JumpBufferDuration = 0.1F;
        jumpBuffer = JumpBufferDuration;
    }
    dropRequested = dropRequested || controls.drop;
}

MotionRequest CatController::motionRequest()
{
    if (physics.hasSupport() && (dropRequested || jumpBuffer > 0)) {
        constexpr float DropImpulse = 200;
        if (dropRequested) {
            physics.dropThrough(DropImpulse);
        } else {
            physics.launch(JumpImpulse);
        }
        jumpBuffer = 0;
    }
    dropRequested = false;
    constexpr float RunningSpeed = 600;
    constexpr float GroundAcceleration = 2400;
    constexpr float AirAcceleration = 1200;
    constexpr float GroundBraking = 4800;
    constexpr float AirBraking = 800;
    constexpr float Gravity = 1500;
    const bool supported = physics.hasSupport();
    const float target = static_cast<float>(direction) * RunningSpeed;
    const float acceleration = direction == 0
        ? (supported ? GroundBraking : AirBraking) : (supported ? GroundAcceleration : AirAcceleration);
    return MotionRequest{target, acceleration, Gravity};
}

void CatController::advanceTime(float duration)
{
    // Приземление в этом кадре ещё может использовать сохранённое нажатие прыжка.
    if (jumpBuffer > 0 && physics.hasSupport()) {
        physics.launch(JumpImpulse);
        jumpBuffer = 0;
    }
    jumpBuffer = std::max(0.0F, jumpBuffer - duration);
}

void CatController::cancelJump()
{
    jumpBuffer = 0;
}

}
