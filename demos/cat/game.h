#pragma once

#include "geometry.h"
#include "renderer.h"
#include "resources.h"
#include "texture.h"
#include "camera.h"
#include "math2d.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <vector>

namespace Cat
{
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

class Images
{
public:
    bool load(Engine::Renderer2D& renderer, Engine::ResourceError& error);
    void draw(Engine::Renderer2D& renderer, std::size_t image, Engine::Vector2 position,
        float scale = 1, float angle = 0) const;

private:
    std::array<Engine::Texture, ArtCount> textures;
};

struct Decoration
{
    Art art = Stone;
    Engine::Vector2 offset;
};

struct Platform
{
    double prepareMotion(double duration);
    void draw(Engine::Renderer2D& renderer, const Images& images, const Engine::Rect& visible) const;

    int id = -1;
    Engine::Vector2 position;
    Art art = Stone;
    std::vector<Decoration> decorations;
    double speed = 0; // Пиксели в секунду: положительная скорость направлена вниз.
    double top = 0;
    double bottom = 0;
};

class Cat
{
public:
    Engine::Vector2 position() const;
    bool standsOn(const Platform& platform) const;
    // duration >= 0; кот и лифты продвигаются по одной шкале времени.
    void advance(std::vector<Platform>& platforms, double duration, const Controls& controls);
    void draw(Engine::Renderer2D& renderer, const Images& images) const;

private:
    struct Range
    {
        double left;
        double right;
    };

    struct Probe
    {
        Probe(double x, double speed, double after = 0, bool allowEntry = true, bool ahead = false);

        double x;
        double speed;
        double after;
        bool allowEntry;
        bool ahead;
    };

    std::optional<Range> supportRange(const Platform& platform, const std::vector<Platform>& platforms, Probe probe) const;
    const Platform* findSupport(const std::vector<Platform>& platforms, int direction = 0, int excluded = -1) const;
    void ignoreSupports(const std::vector<Platform>& platforms);
    void leaveEdge(const Platform& platform, const std::vector<Platform>& platforms, double x);
    double move(std::vector<Platform>& platforms, double duration, int direction);
    void returnToTop();
    void animate(double duration, int direction);

    Engine::Vector2 feet = {450, 223.75};
    Engine::Vector2 velocity;
    int support = -1;
    int entryDirection = 0; // 0 — устойчивая опора, +1 — въезд слева, -1 — справа.
    // Покинутые площадки снова участвуют в столкновениях после прохода сквозь них или приземления.
    std::set<int> ignored;
    double jumpBuffer = 0;
    bool dropRequested = false;
    int headFrame = (Feet - Head) / 2;
    int feetFrame = 0;
    double animationTime = 0;
    double frameTime = 0;
};

struct Leaf
{
    void draw(Engine::Renderer2D& renderer, const Images& images, const Engine::Rect& visible) const;

    Art art = Leaf1;
    Engine::Vector2 position;
    double angle = 0; // Градусы.
    double speed = 0; // Пиксели в секунду.
    double spin = 0; // Градусы в секунду.
    bool active = false;
};

class Game
{
public:
    Game(std::uint32_t routeSeed, std::uint32_t leavesSeed);
    // После update(), до draw(): положительные размеры окна и камера, следующая за котом.
    bool prepareView(int width, int height, Engine::CameraError& error);
    void update(double duration, const Controls& controls);
    void draw(Engine::Renderer2D& renderer, const Images& images) const;

private:
    enum class Pattern
    {
        Normal,
        Lift,
        Zigzag
    };

    static int random(std::uint32_t& state);
    void addPlatform(int x, int y, Art art, double top = 0, double bottom = 0);
    void decorate(Platform& platform);
    bool overlaps(int x, int y) const;
    void generate(double catX, double horizon);
    void generateNormal(double catX, double horizon);
    bool generateSpecial(double catX, bool zigzag);
    void updateLeaves(double duration, double catX);

    Cat cat;
    std::vector<Platform> platforms;
    std::array<Leaf, 50> leaves;
    std::uint32_t routeRandom;
    std::uint32_t leavesRandom;
    int nextId = 0;
    Pattern pattern = Pattern::Normal;
    int nextPatternX = 3000;
    int lastX = 360;
    int lastY = 400;
    double time = 0;
    Engine::CameraView view;
    double visibleWidth = 935;
};
}
