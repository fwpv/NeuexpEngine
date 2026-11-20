#include "cat.h"
#include "game.h"
#include "graphics/renderer.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace Cat
{
namespace
{
constexpr float JumpImpulse = 810;
}

Cat::Cat(const Images& images)
    : head(images.headAnimation())
    , feet(images.feetAnimation())
{
    body.position = NeuexpEngine::Vector2{450, 223.75F};
    body.supportShape.segments = std::array<Interval, 2>{Interval{-60.75F, -20.25F}, Interval{8.25F, 54.75F}};
    body.supportShape.overhang = 10;
    body.supportShape.heightAllowance = 5;
    body.supportShape.minimumOverlap = 3;
}

NeuexpEngine::Vector2 Cat::position() const
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
    for (NeuexpEngine::Animation* animation : {&head, &feet}) {
        if (direction == 0) {
            animation->pause();
            animation->setFrame(animation->currentFrame());
        } else {
            animation->setDirection(direction > 0 ? NeuexpEngine::AnimationDirection::Forward : NeuexpEngine::AnimationDirection::Backward);
            animation->play();
            animation->update(duration);
        }
    }
}

void Cat::draw(NeuexpEngine::Renderer2D& renderer) const
{
    const NeuexpEngine::Vector2 origin = body.position + NeuexpEngine::Vector2{-101.25F, -123.75F + 3 + (grounded ? 2 : 0)};
    NeuexpEngine::DrawOptions options;
    options.transform = NeuexpEngine::Transform2{origin, 0, NeuexpEngine::Vector2{0.75F, 0.75F}, NeuexpEngine::Vector2{}}.toAffine();
    renderer.drawSprite(head, options);
    options.transform = NeuexpEngine::Transform2{origin + NeuexpEngine::Vector2{44 * 0.75F, 120 * 0.75F},
        0, NeuexpEngine::Vector2{0.75F, 0.75F}, NeuexpEngine::Vector2{}}.toAffine();
    renderer.drawSprite(feet, options);
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
