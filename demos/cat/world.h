#pragma once

#include "geometry.h"
#include "math2d.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <vector>

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

class Cat
{
public:
    Engine::Vector2 position() const;
    bool standsOn(const Platform& platform) const;
    // duration >= 0; кот и лифты продвигаются по одной шкале времени.
    void advance(std::vector<Platform>& platforms, float duration, const Controls& controls);
    void draw(Engine::Renderer2D& renderer, const Images& images) const;

private:
    struct Range
    {
        float left;
        float right;
    };

    struct Probe
    {
        Probe(float x, float speed, float after = 0, bool allowEntry = true, bool ahead = false);

        float x;
        float speed;
        float after;
        bool allowEntry;
        bool ahead;
    };

    std::optional<Range> supportRange(const Platform& platform, const std::vector<Platform>& platforms, Probe probe) const;
    const Platform* findSupport(const std::vector<Platform>& platforms, int direction = 0, int excluded = -1) const;
    void ignoreSupports(const std::vector<Platform>& platforms);
    void leaveEdge(const Platform& platform, const std::vector<Platform>& platforms, float x);
    float move(std::vector<Platform>& platforms, float duration, int direction);
    void returnToTop();
    void animate(float duration, int direction);

    Engine::Vector2 feet = {450, 223.75F};
    Engine::Vector2 velocity;
    int support = -1;
    int entryDirection = 0; // 0 — устойчивая опора, +1 — въезд слева, -1 — справа.
    // Покинутые площадки снова участвуют в столкновениях после прохода сквозь них или приземления.
    std::set<int> ignored;
    float jumpBuffer = 0;
    bool dropRequested = false;
    int headFrame = (Feet - Head) / 2;
    int feetFrame = 0;
    float animationTime = 0;
    float frameTime = 0;
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
