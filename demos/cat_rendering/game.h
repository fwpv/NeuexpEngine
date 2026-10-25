#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Cat
{
constexpr int CanvasWidth = 935;
constexpr int CanvasHeight = 700;
constexpr double CatScreenX = 315;
constexpr double FeetOffset = 158;
constexpr int PlatformWidth = 175;
constexpr int PlatformHeight = 35;
constexpr std::size_t PlatformLimit = 50;

enum class Art { Platform, MovingPlatform, GrassPlatform, Mushroom, Flowers, Grass,
    Head1, Head2, Head3, Head4, Head5, Feet, Leaf1, Leaf2, Leaf3, Font, Count };

struct Controls
{
    bool right = false;
    bool left = false;
    bool up = false;
    bool down = false;
};

struct Decoration
{
    Art art = Art::Grass;
    int x = 0;
    int y = 0;
};

struct Platform
{
    double x = 0;
    double y = 0;
    Art art = Art::Platform;
    std::vector<Decoration> decorations;
    double speed = 0;
    int bottom = 0;
    int top = 0;
    bool movingDown = false;
};

struct Leaf
{
    Art art = Art::Leaf1;
    double x = 0;
    double y = 0;
    double angle = 0;
    double speed = 0;
    double spin = 0;
    bool active = false;
};

struct Player
{
    double x = CatScreenX;
    double y = 100;
    double rightSpeed = 0;
    double leftSpeed = 0;
    double fallSpeed = 0;
    double jumpSpeed = 0;
    double previousY = 0;
    double currentY = 0;
    bool grounded = false;
    int feet = 1;
    int head = 3;
};

class Game
{
public:
    Game();
    void update(int milliseconds, const Controls& controls);

    Player player;
    std::array<std::optional<Platform>, PlatformLimit> platforms;
    std::array<Leaf, 50> leaves;
    double cameraX = 0;
    std::uint32_t time = 0;

private:
    std::uint32_t randomState = 1;
    int generationMode = 1;
    int nextModeX = 3000;
    int lastX = 360;
    int lastY = 400;
    std::uint32_t feetTime = 0;
    std::uint32_t headTime = 0;

    int random();
    std::size_t platformCount() const;
    void addPlatform(int x, int y, Art art, int bottom = 0, int top = 0);
    void decorate(Platform& platform);
    bool overlaps(int x, int y) const;
    void generate();
    void generateNormal(int type);
    bool generateLift();
    bool generateZigzag();
    void movePlatform(Platform& platform, double y);
    void updatePlatforms(int milliseconds);
    void updateLeaves(int milliseconds);
    void updatePlayer(int milliseconds, const Controls& controls);
    bool land(double x, double previousFeet, double feet);
};
}
