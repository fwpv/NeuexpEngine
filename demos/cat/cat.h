#pragma once

#include "math2d.h"

#include <cstddef>
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
struct Platform;

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

    struct Motion
    {
        Engine::Vector2 positionAt(float time) const;

        Engine::Vector2 origin;
        Engine::Vector2 velocity;
        Engine::Vector2 acceleration;
        float targetSpeed;
        float targetTime;
        float duration;
    };

    struct Contact
    {
        enum class Kind
        {
            None,
            Landing,
            Departure,
            ReturnToTop
        };

        float time;
        Kind kind = Kind::None;
        const Platform* platform = nullptr;
        float edgeX = 0;
    };

    std::vector<Range> supportRanges(const Platform& platform, const std::vector<Platform>& platforms, Probe probe) const;
    std::optional<Range> supportRange(const Platform& platform, const std::vector<Platform>& platforms, Probe probe) const;
    const Platform* findSupport(const std::vector<Platform>& platforms, int direction = 0, int excluded = -1) const;
    void retireIgnoredPlatforms(const std::vector<Platform>& platforms);
    void applySupportCommands(const std::vector<Platform>& platforms);
    void ignoreSupports(const std::vector<Platform>& platforms);
    void leaveEdge(const Platform& platform, const std::vector<Platform>& platforms, float x);
    Motion planMotion(float duration, int direction, const Platform* standing, bool entering) const;
    void limitSupportedMotion(const std::vector<Platform>& platforms, const Platform& standing,
        bool entering, const Motion& motion, Contact& contact) const;
    void findAirborneContact(const std::vector<Platform>& platforms, const Motion& motion, Contact& contact) const;
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

}
