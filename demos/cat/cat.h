#pragma once

#include "physics.h"
#include "animation.h"
#include "math2d.h"

#include <cstddef>

namespace NeuexpEngine
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
    ArtCount
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
    explicit Cat(const Images& images);
    NeuexpEngine::Vector2 position() const;
    Body& physicalBody();
    void updateAnimation(float duration, const Controls& controls, bool supported);
    void draw(NeuexpEngine::Renderer2D& renderer) const;

private:
    Body body;
    bool grounded = false;
    NeuexpEngine::Animation head;
    NeuexpEngine::Animation feet;
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
