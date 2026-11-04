#pragma once

#include "world.h"
#include "camera.h"
#include "math2d.h"
#include "resources.h"
#include "sprite.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Cat
{
class Images
{
public:
    bool load(Engine::Renderer2D& renderer, Engine::ResourceError& error);
    void draw(Engine::Renderer2D& renderer, std::size_t image, Engine::Vector2 position,
        float scale = 1, float angle = 0) const;

private:
    std::array<Engine::Sprite, ArtCount> sprites;
};

class Game
{
public:
    Game(std::uint32_t routeSeed, std::uint32_t leavesSeed);
    // После update(), до draw(): положительные размеры окна и камера, следующая за котом.
    bool prepareView(int width, int height, Engine::CameraError& error);
    void update(float duration, const Controls& controls);
    void draw(Engine::Renderer2D& renderer, const Images& images) const;

private:
    World world;
    Foliage foliage;
    Engine::CameraView view;
    float visibleWidth = 935;
};
}
