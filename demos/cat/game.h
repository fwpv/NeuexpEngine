#pragma once

#include "world.h"
#include "core/camera.h"
#include "core/math2d.h"
#include "core/resources.h"
#include "graphics/sprite.h"
#include "graphics/animation.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Cat
{
class Images
{
public:
    bool load(NeuexpEngine::Renderer2D& renderer, NeuexpEngine::ResourceError& error);
    void draw(NeuexpEngine::Renderer2D& renderer, std::size_t image, NeuexpEngine::Vector2 position,
        float scale = 1, float angle = 0) const;

    const NeuexpEngine::Animation& headAnimation() const { return head; }
    const NeuexpEngine::Animation& feetAnimation() const { return feet; }

private:
    std::array<NeuexpEngine::Sprite, ArtCount> sprites;
    NeuexpEngine::Animation head;
    NeuexpEngine::Animation feet;
};

class Game
{
public:
    Game(std::uint32_t routeSeed, std::uint32_t leavesSeed, const Images& images);
    // После update(), до draw(): положительные размеры окна и камера, следующая за котом.
    bool prepareView(int width, int height, NeuexpEngine::CameraError& error);
    void update(float duration, const Controls& controls);
    bool isOver() const;
    double distanceTravelled() const { return distance; }
    void draw(NeuexpEngine::Renderer2D& renderer, const Images& images) const;

private:
    World world;
    Foliage foliage;
    NeuexpEngine::CameraView view;
    float visibleWidth = 935;
    double distance = 0;
};
}
