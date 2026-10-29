#pragma once

#include "renderer.h"
#include "resources.h"
#include "texture.h"
#include "ui.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <vector>

namespace Cat
{
// Мировые координаты в пикселях: X вправо, Y вниз. Время — в секундах.
struct Point
{
    double x;
    double y;
};

struct Vector
{
    double x;
    double y;
};

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
    bool right;
    bool left;
    bool jump;
    bool drop;
};

class Images
{
public:
    bool load(Engine::Renderer2D& renderer, Engine::ResourceError& error);
    void draw(Engine::Renderer2D& renderer, std::size_t image, Point screen,
        double scale = 1, double angle = 0) const;

private:
    std::array<Engine::Texture, ArtCount> textures;
};

struct Decoration
{
    Art art;
    Point offset;
};

struct Platform
{
    double prepareMotion(double duration);
    void draw(Engine::Renderer2D& renderer, const Images& images, double camera, double width) const;

    int id;
    Point position;
    Art art;
    std::vector<Decoration> decorations;
    double speed; // Пиксели в секунду: положительная скорость направлена вниз.
    double top;
    double bottom;
};

class Cat
{
public:
    Cat();
    Point position() const;
    bool standsOn(const Platform& platform) const;
    // duration >= 0; кот и лифты продвигаются по одной шкале времени.
    void advance(std::vector<Platform>& platforms, double duration, const Controls& controls);
    void draw(Engine::Renderer2D& renderer, const Images& images, double camera) const;

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

    Point feet;
    Vector velocity;
    int support;
    int entryDirection; // 0 — устойчивая опора, +1 — въезд слева, -1 — справа.
    // Покинутые площадки снова участвуют в столкновениях после прохода сквозь них или приземления.
    std::set<int> ignored;
    double jumpBuffer;
    Controls previousControls;
    bool dropRequested;
    int headFrame;
    int feetFrame;
    double animationTime;
    double frameTime;
};

struct Leaf
{
    Leaf();
    void draw(Engine::Renderer2D& renderer, const Images& images, double camera, double width) const;

    Art art;
    Point position;
    double angle; // Градусы.
    double speed; // Пиксели в секунду.
    double spin; // Градусы в секунду.
    bool active;
};

class Game
{
public:
    Game(std::uint32_t routeSeed, std::uint32_t leavesSeed);
    // Положительные размеры клиентской области; вид задаётся до первого рисования.
    bool resize(int width, int height, Engine::UiError& error);
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
    int nextId;
    Pattern pattern;
    int nextPatternX;
    int lastX;
    int lastY;
    double time;
    Engine::UiView view;
    double visibleWidth;
};
}
