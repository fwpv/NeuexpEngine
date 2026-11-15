#pragma once

#include "cat.h"
#include "physics.h"
#include "geometry.h"
#include "math2d.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace NeuexpEngine
{
class Renderer2D;
}

namespace Cat
{
class Images;

struct Decoration
{
    Art art = Stone;
    NeuexpEngine::Vector2 offset;
};

struct Platform
{
    Platform(int id, NeuexpEngine::Vector2 position, Art art);
    void draw(NeuexpEngine::Renderer2D& renderer, const Images& images, const NeuexpEngine::Rect& visible) const;

    static constexpr NeuexpEngine::Size2 DefaultSize{175, 35};

    Surface surface;
    Art art = Stone;
    std::vector<Decoration> decorations;
};

struct Leaf
{
    void draw(NeuexpEngine::Renderer2D& renderer, const Images& images, const NeuexpEngine::Rect& visible) const;

    Art art = Leaf1;
    NeuexpEngine::Vector2 position;
    float angle = 0; // Градусы.
    float speed = 0; // Пиксели в секунду.
    float spin = 0; // Градусы в секунду.
    bool active = false;
};

class World
{
public:
    World(std::uint32_t seed, const Images& images);
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = delete;
    World& operator=(World&&) = delete;
    NeuexpEngine::Vector2 catPosition() const;
    void update(float duration, const Controls& controls, float visibleWidth);
    void draw(NeuexpEngine::Renderer2D& renderer, const Images& images, const NeuexpEngine::Rect& visible) const;

private:
    enum class Pattern
    {
        Normal,
        Lift,
        Zigzag
    };

    void addPlatform(int x, int y, Art art, float top = 0, float bottom = 0);
    void decorate(Platform& platform);
    void updateMotion(float duration, const Controls& controls);
    bool overlaps(int x, int y) const;
    void generate(float catX, float horizon);
    void generateNormal(float catX, float horizon);
    bool generateSpecial(float catX, bool zigzag);

    Cat cat;
    std::vector<Platform> platforms;
    std::vector<Surface*> surfaces;
    CatPhysics physics;
    CatController controller;
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
    void draw(NeuexpEngine::Renderer2D& renderer, const Images& images, const NeuexpEngine::Rect& visible) const;

private:
    std::array<Leaf, 50> leaves;
    std::uint32_t leavesRandom;
    float time = 0;
};
}
