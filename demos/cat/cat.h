#pragma once

#include "physics.h"
#include "math2d.h"

#include <cstddef>

namespace Engine
{
class Renderer2D;
}

namespace Cat
{
class Images;

enum Art : std::size_t
{
    Stone,
    Lift,
    Turf,
    Mushroom,
    Flowers,
    Grass,
    Leaf1,
    Leaf2,
    Leaf3,
    Head,
    Feet = Head + 5,
    ArtCount = Feet + 12
};

struct Controls
{
    bool right = false;
    bool left = false;
    bool jump = false; // Нажатие за текущий кадр.
    bool drop = false; // Нажатие за текущий кадр.
};

class Cat
{
public:
    Cat();
    Engine::Vector2 position() const;
    Body& physicalBody();
    void updateAnimation(float duration, const Controls& controls, bool supported);
    void draw(Engine::Renderer2D& renderer, const Images& images) const;

private:
    Body body;
    bool grounded = false;
    int headFrame = (Feet - Head) / 2;
    int feetFrame = 0;
    float animationTime = 0;
    float frameTime = 0;
};

class CatController
{
public:
    explicit CatController(CatPhysics& physics);
    void beginFrame(const Controls& controls);
    MotionRequest motionRequest();
    void advanceTime(float duration);
    void cancelJump();

private:
    CatPhysics& physics;
    int direction = 0;
    float jumpBuffer = 0;
    bool dropRequested = false;
};

}
