#pragma once

#include "cat.h"
#include "geometry.h"
#include "math2d.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Engine
{
class Renderer2D;
}

namespace Cat
{
class Images;

struct Decoration
{
    Art art = Stone;
    Engine::Vector2 offset;
};

struct Platform
{
    float prepareMotion(float duration);
    void draw(Engine::Renderer2D& renderer, const Images& images, const Engine::Rect& visible) const;

    int id = -1;
    Engine::Vector2 position;
    Art art = Stone;
    std::vector<Decoration> decorations;
    float speed = 0; // Пиксели в секунду: положительная скорость направлена вниз.
    float top = 0;
    float bottom = 0;
};

struct Leaf
{
    void draw(Engine::Renderer2D& renderer, const Images& images, const Engine::Rect& visible) const;

    Art art = Leaf1;
    Engine::Vector2 position;
    float angle = 0; // Градусы.
    float speed = 0; // Пиксели в секунду.
    float spin = 0; // Градусы в секунду.
    bool active = false;
};

class World
{
public:
    explicit World(std::uint32_t seed);
    Engine::Vector2 catPosition() const;
    void update(float duration, const Controls& controls, float visibleWidth);
    void draw(Engine::Renderer2D& renderer, const Images& images, const Engine::Rect& visible) const;

private:
    enum class Pattern
    {
        Normal,
        Lift,
        Zigzag
    };

    void addPlatform(int x, int y, Art art, float top = 0, float bottom = 0);
    void decorate(Platform& platform);
    bool overlaps(int x, int y) const;
    void generate(float catX, float horizon);
    void generateNormal(float catX, float horizon);
    bool generateSpecial(float catX, bool zigzag);

    Cat cat;
    std::vector<Platform> platforms;
    std::uint32_t routeRandom;
    int nextId = 0;
    Pattern pattern = Pattern::Normal;
    int nextPatternX = 3000;
    int lastX = 360;
    int lastY = 400;
};

class Foliage
{
public:
    explicit Foliage(std::uint32_t seed);
    void update(float duration, float catX);
    void draw(Engine::Renderer2D& renderer, const Images& images, const Engine::Rect& visible) const;

private:
    std::array<Leaf, 50> leaves;
    std::uint32_t leavesRandom;
    float time = 0;
};
}
