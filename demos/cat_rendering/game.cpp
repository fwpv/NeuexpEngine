#include "game.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Cat
{
namespace
{
constexpr double Gravity = 0.0015;
constexpr double JumpSpeed = -0.81;
constexpr double GroundAcceleration = 0.001;
constexpr double AirFriction = 0.00035;
constexpr double GroundFriction = 0.004;
constexpr double InitialWalkSpeed = 0.3;
constexpr double WalkThreshold = 0.3;
constexpr double MaximumWalkSpeed = 0.6;
constexpr int AnimationInterval = 30;
constexpr int GenerationAhead = 1024;

int artHeight(Art art)
{
    switch (art) {
    case Art::Mushroom: return 58;
    case Art::Flowers: return 80;
    case Art::Grass: return 35;
    default: return 0;
    }
}
}

Game::Game()
{
    addPlatform(lastX, lastY, Art::Platform);
    // CAT.exe 0x401000: последовательность MSVCR100 rand() начинается с seed=1.
    for (auto& leaf : leaves) {
        const int type = random() % 8;
        leaf.art = type == 7 ? Art::Leaf1 : (type % 3 == 0 ? Art::Leaf2 : Art::Leaf3);
    }
}

int Game::random()
{
    randomState = randomState * 214013U + 2531011U;
    return static_cast<int>((randomState >> 16U) & 32767U);
}

std::size_t Game::platformCount() const
{
    return static_cast<std::size_t>(std::count_if(platforms.begin(), platforms.end(),
        [](const auto& platform) { return platform.has_value(); }));
}

void Game::addPlatform(int x, int y, Art art, int bottom, int top)
{
    const auto slot = std::find_if(platforms.begin(), platforms.end(),
        [](const auto& platform) { return !platform.has_value(); });
    if (slot == platforms.end()) { return; }
    Platform platform;
    platform.x = x;
    platform.y = y;
    platform.art = art;
    platform.bottom = bottom;
    platform.top = top;
    if (art == Art::MovingPlatform) {
        platform.speed = std::min(0.4, 0.2 + (1 - std::pow(5.0, -player.x / 100000)) * 0.2);
    }
    *slot = std::move(platform);
}

// CAT.exe 0x403830: декорации не являются предметами для подбора.
void Game::decorate(Platform& platform)
{
    const auto add = [&](Art art, int x) {
        platform.decorations.push_back({ art, x, 2 - artHeight(art) });
    };
    switch (random() % 8) {
    case 0:
        switch (random() % 5 + 1) {
        case 1: add(Art::Mushroom, random() % 52 + 31); break;
        case 2: {
            const int first = random() % 16;
            add(Art::Flowers, first);
            add(Art::Flowers, random() % 16 + 88);
            break;
        }
        case 5: {
            int first = 0;
            int second = 0;
            if (random() % 2 == 0) {
                first = random() % 30;
                second = random() % (70 - first) + first + 64;
            } else {
                second = random() % 30;
                first = random() % (60 - second) + second + 75;
            }
            add(Art::Mushroom, first - 10);
            add(Art::Flowers, second);
            break;
        }
        default: break;
        }
        break;
    case 1: case 2: case 3: add(Art::Flowers, 52); break;
    case 4: case 5: add(Art::Grass, random() % 135); break;
    case 6: add(Art::Flowers, random() % 16 + 36); break;
    case 7: {
        const int first = random() % 48;
        add(Art::Grass, first);
        add(Art::Grass, random() % 48 + 88);
        break;
    }
    }
}

bool Game::overlaps(int x, int y) const
{
    for (const auto& slot : platforms) {
        if (!slot) { continue; }
        const int px = static_cast<int>(slot->x);
        const int py = static_cast<int>(slot->y);
        if (x + PlatformWidth > px && y + PlatformHeight > py
            && x < px + PlatformWidth && y < py + PlatformHeight) {
            return true;
        }
    }
    return false;
}

// CAT.exe 0x4030b0: ограничения на расстояние проверяются до добавления платформы.
void Game::generateNormal(int type)
{
    random();
    random();
    random();
    if (lastX >= player.x + GenerationAhead || platformCount() == PlatformLimit) { return; }
    const int gap = static_cast<int>((1 - std::pow(25.0, -player.x / 100000)) * 275 + 175);
    const int span = player.x > 100000 ? 460 : static_cast<int>(gap + 40 - 30 * player.x / 100000);
    do {
        const int y = type == 1 ? random() % 500 + 150 : (random() % 100) * 5 + 150;
        const int offset = random() % span;
        const int x = lastX + random() % 200 + offset;
        const int dy = y - lastY;
        const int distanceY = std::abs(dy);
        if (distanceY >= 195) { continue; }
        const int distance = lastX - x + span;
        if (x >= span && dy * dy + distance * distance >= 40000 && distance < 0) { continue; }
        if (type == 1) {
            if (distanceY <= 185 && distance > 0) { continue; }
        } else if (distanceY <= 185) { continue; }
        const int shortDistance = lastX - x + gap;
        if (x <= gap && dy * dy + shortDistance * shortDistance <= 40000 && shortDistance < 0) { continue; }
        if (overlaps(x, y)) { continue; }
        const bool plain = random() % 2 == 0;
        addPlatform(x, y, plain ? Art::Platform : Art::GrassPlatform);
        if (!plain) {
            const auto slot = std::find_if(platforms.begin(), platforms.end(),
                [&](const auto& platform) { return platform && platform->x == x && platform->y == y; });
            decorate(**slot);
        }
        lastX = x;
        lastY = y;
    } while (lastX < player.x + GenerationAhead && platformCount() < PlatformLimit);
}

bool Game::generateLift()
{
    if (lastX >= player.x + GenerationAhead || platformCount() >= PlatformLimit - 2) { return false; }
    const int bottom = std::min(lastY + 400, 650);
    int x = lastX + random() % 50 + 250;
    addPlatform(x, bottom, Art::Platform);
    x += 200;
    const int top = random() % 200 + 150;
    addPlatform(x, 300, Art::MovingPlatform, bottom, top);
    x += 200;
    addPlatform(x, top, Art::Platform);
    lastX = x;
    lastY = top;
    return true;
}

bool Game::generateZigzag()
{
    const double difficulty = std::min(1.0, 1 - std::pow(5.0, -player.x / 100000));
    if (lastX >= player.x + GenerationAhead || platformCount() >= PlatformLimit - 4) { return false; }
    int middleX = 0;
    for (int index = 0; index < 4; ++index) {
        int x = static_cast<int>(lastX + 320 + 200 * difficulty);
        const int y = index % 2 ? 650 : 150;
        if (index == 2) {
            middleX = x;
            x = static_cast<int>(lastX + 640 + 400 * difficulty);
        }
        addPlatform(x, y, Art::MovingPlatform, static_cast<int>(650 - 100 * difficulty), 150);
        lastX = x;
        lastY = y;
    }
    addPlatform(middleX, 300, Art::GrassPlatform);
    lastX += 320;
    lastY = 300;
    addPlatform(lastX, lastY, Art::Platform);
    return true;
}

void Game::generate()
{
    if (player.x > nextModeX) {
        if ((player.x > 3000 && player.x < 20000) || player.x > 30000) {
            generationMode = random() % 3 == 0 ? 3 : 2;
            nextModeX += random() % 1000 + (generationMode == 3 ? 3000 : 1500);
        }
        if (player.x > 20000 && player.x < 30000) {
            generationMode = random() % 3 != 0 ? 3 : 2;
            nextModeX += random() % 1000 + (generationMode == 3 ? 3000 : 1500);
        }
    }
    if (generationMode == 1) {
        const int type = random() % 2 + 1;
        generateNormal(type);
    } else if (generationMode == 2 ? generateLift() : generateZigzag()) {
        generationMode = 1;
    }
}

void Game::movePlatform(Platform& platform, double y)
{
    if (player.jumpSpeed == 0 && player.x + 135 >= platform.x - 10
        && player.x + 135 <= platform.x + PlatformWidth + 20) {
        const double feet = player.y + FeetOffset;
        const double carriedY = player.y + y - platform.y;
        if (feet + 2 >= y && platform.y >= feet - 15) {
            player.y = carriedY;
            player.currentY = player.y - 50;
            player.fallSpeed = 0;
            player.grounded = true;
        }
        if (y >= feet - 2 && platform.y <= feet + 2) {
            player.y = carriedY;
            player.currentY = player.y - 50;
        }
    }
    platform.y = y;
}

void Game::updatePlatforms(int milliseconds)
{
    generate();
    for (auto& slot : platforms) {
        if (slot && slot->x <= player.x - GenerationAhead) { slot.reset(); }
    }
    for (auto& slot : platforms) {
        if (!slot || slot->art != Art::MovingPlatform) { continue; }
        auto& platform = *slot;
        const double movement = milliseconds * platform.speed;
        movePlatform(platform, platform.y + (platform.movingDown ? movement : -movement));
        if (platform.movingDown && platform.y > platform.bottom) {
            platform.y = platform.bottom;
            platform.movingDown = false;
        } else if (!platform.movingDown && platform.y < platform.top) {
            platform.y = platform.top;
            platform.movingDown = true;
        }
    }
}

// CAT.exe 0x401240/0x4014e0: листья обновляются до генерации платформ.
void Game::updateLeaves(int milliseconds)
{
    for (auto& leaf : leaves) {
        if (leaf.x >= player.x - 1050 && leaf.x <= player.x + 3000 && leaf.y <= 725 && leaf.active) {
            const float phase = static_cast<float>(time / 7000.0);
            const float wind = static_cast<float>(std::sin(phase));
            leaf.x -= milliseconds * (wind / 10.0 + leaf.speed * 0.25);
            leaf.y += milliseconds * leaf.speed;
            leaf.angle += milliseconds * leaf.spin;
        } else {
            leaf.active = false;
        }
    }
    for (auto& leaf : leaves) {
        if (leaf.active) { continue; }
        leaf.y = random() % 350 - 400;
        leaf.x = static_cast<int>(random() % 3770 + player.x - 1050);
        leaf.speed = (random() % 15 / 100.0 + 0.1) / 1.5;
        leaf.angle = 0;
        leaf.spin = (random() % 4 - 1) / 20.0;
        leaf.active = true;
    }
}

bool Game::land(double x, double previousFeet, double feet)
{
    for (const auto& slot : platforms) {
        if (!slot) { continue; }
        const int px = static_cast<int>(slot->x);
        const int py = static_cast<int>(slot->y);
        if (x >= px - 10 && x <= px + PlatformWidth + 20 && previousFeet <= py + 7 && feet >= py - 1) {
            player.y = py - 159;
            return true;
        }
    }
    return false;
}

// CAT.exe 0x4049c0: скорости заданы в пикселях/мс; подъём и падение хранятся отдельно.
void Game::updatePlayer(int milliseconds, const Controls& controls)
{
    const double dt = std::min(milliseconds, 200);
    if (player.jumpSpeed == 0) {
        player.fallSpeed += dt * Gravity;
        player.y += player.fallSpeed * dt;
        player.previousY = player.currentY;
        player.currentY = player.y;
        if (!controls.down && land(player.x + 135, player.previousY + FeetOffset, player.currentY + FeetOffset)) {
            player.fallSpeed = 0;
            player.grounded = true;
        }
    }
    player.jumpSpeed = std::min(0.0, player.jumpSpeed + dt * Gravity);
    player.y += player.jumpSpeed * dt;
    if (player.fallSpeed == 0 && player.grounded && controls.up) {
        player.grounded = false;
        player.jumpSpeed = JumpSpeed;
    }
    const auto walk = [&](double& speed, bool held) {
        if (held) {
            if (speed < WalkThreshold) {
                speed = InitialWalkSpeed;
            } else if (speed < MaximumWalkSpeed && player.grounded) {
                speed += dt * GroundAcceleration;
            }
        } else if (speed > 0) {
            speed -= dt * (player.grounded ? GroundFriction : AirFriction);
        } else {
            speed = 0;
        }
    };
    walk(player.rightSpeed, controls.right);
    walk(player.leftSpeed, controls.left);
    if (controls.down && player.grounded) {
        player.grounded = false;
        player.y += 9;
        player.fallSpeed = 0.5;
    }
    player.x += std::abs(player.rightSpeed * dt) - std::abs(player.leftSpeed * dt);
    if (player.y > 2000) {
        player.y = 0;
        player.fallSpeed = 0.5;
    }
}

void Game::update(int milliseconds, const Controls& controls)
{
    time += static_cast<std::uint32_t>(milliseconds);
    cameraX = player.x - CatScreenX;
    random();
    updateLeaves(milliseconds);
    updatePlatforms(milliseconds);
    if (time > headTime + AnimationInterval) {
        if (controls.left && player.head != 1) { --player.head; }
        if (controls.right && player.head != 5) { ++player.head; }
        headTime = time;
    }
    if (time > feetTime + AnimationInterval) {
        if (controls.left && --player.feet == 0) { player.feet = 12; }
        if (controls.right && ++player.feet == 13) { player.feet = 1; }
        feetTime = time;
    }
    if (!controls.left && !controls.right) { feetTime = time; }
    updatePlayer(milliseconds, controls);
}
}
