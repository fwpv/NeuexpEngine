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
constexpr double CatScale = 0.75;
constexpr double SupportPointOffset = 135 * CatScale;
constexpr double CatScreenX = 450 - SupportPointOffset;
constexpr double FeetOffset = 165 * CatScale;
constexpr int PlatformWidth = 175;
constexpr int PlatformHeight = 35;
constexpr std::size_t PlatformLimit = 50;

enum class Art { Platform, MovingPlatform, GrassPlatform, Mushroom, Flowers, Grass,
    Head1, Head2, Head3, Head4, Head5, Feet, Leaf1, Leaf2, Leaf3, Count };

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
    // Скорости в пикселях в секунду; положительная вертикальная скорость направлена вниз.
    double velocityX = 0;
    double velocityY = 0;
    bool grounded = false;
    int feet = 1;
    int head = 3;
};

class Game
{
public:
    Game();
    // Время кадра задаётся в секундах.
    void update(double deltaTime, const Controls& controls);

    Player player;
    std::array<std::optional<Platform>, PlatformLimit> platforms;
    std::array<Leaf, 50> leaves;
    double cameraX = 0;
    double viewWidth = CanvasWidth;
    double time = 0;

private:
    std::uint32_t randomState = 1;
    int generationMode = 1;
    int nextModeX = 3000;
    int lastX = 360;
    int lastY = 400;
    std::uint32_t leafRandomState = 7;
    double jumpBufferTime = 0;
    bool jumpHeld = false;
    bool downHeld = false;
    bool dropRequested = false;
    std::optional<std::size_t> support;
    std::optional<std::size_t> ignoredPlatform;
    int fallDirection = 0;
    double cameraOffsetX = 0;
    double feetTime = 0;
    double headTime = 0;

    int random();
    int leafRandom();
    std::size_t platformCount() const;
    void addPlatform(int x, int y, Art art, int bottom = 0, int top = 0);
    void decorate(Platform& platform);
    bool overlaps(int x, int y) const;
    void generate();
    void generateNormal(int type);
    bool generateLift();
    bool generateZigzag();
    void generatePlatforms();
    void updateLeaves(double deltaTime);
    void updatePlayer(double deltaTime, const Controls& controls);
    void updateAnimation(const Controls& controls);
    bool supportsPlayer(const Platform& platform, double playerX) const;
    double generationRight() const;
    void respawn();
};
}
