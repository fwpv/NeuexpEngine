#include "game.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Cat
{
namespace
{
constexpr double Gravity = 1500;
constexpr double JumpSpeed = -810;
constexpr double GroundAcceleration = 2400;
constexpr double AirAcceleration = 1200;
constexpr double GroundBraking = 4800;
constexpr double AirBraking = 800;
constexpr double MaximumWalkSpeed = 600;
constexpr double ContactTolerance = 0.000001;
constexpr double TimeTolerance = 0.000000001;
constexpr double DropSpeed = 200;
constexpr double RespawnFallSpeed = 500;
constexpr double JumpBufferDuration = 0.1;
constexpr double AnimationInterval = 0.03;
constexpr double EdgeSlideSpeed = 80;
constexpr double CameraReturnRate = 10;
constexpr int GenerationAhead = 1024;

double platformVelocityY(const Platform& platform)
{
    return platform.art == Art::MovingPlatform && platform.bottom > platform.top
        ? (platform.movingDown ? platform.speed : -platform.speed) : 0;
}

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
    for (auto& leaf : leaves) {
        const int type = leafRandom() % 8;
        leaf.art = type == 7 ? Art::Leaf1 : (type % 3 == 0 ? Art::Leaf2 : Art::Leaf3);
    }
}

int Game::random()
{
    randomState = randomState * 214013U + 2531011U;
    return static_cast<int>((randomState >> 16U) & 32767U);
}

int Game::leafRandom()
{
    leafRandomState = leafRandomState * 214013U + 2531011U;
    return static_cast<int>((leafRandomState >> 16U) & 32767U);
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
        platform.speed = std::min(400.0, 200 + (1 - std::pow(5.0, -player.x / 100000)) * 200);
        platform.y = std::clamp(platform.y, static_cast<double>(top), static_cast<double>(bottom));
    }
    *slot = std::move(platform);
}

// Декорации не являются предметами для подбора.
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

// Ограничения на расстояние проверяются до добавления платформы.
void Game::generateNormal(int type)
{
    if (lastX >= generationRight() || platformCount() == PlatformLimit) { return; }
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
    } while (lastX < generationRight() && platformCount() < PlatformLimit);
}

bool Game::generateLift()
{
    if (lastX >= generationRight() || platformCount() >= PlatformLimit - 2) { return false; }
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
    if (lastX >= generationRight() || platformCount() > PlatformLimit - 6) { return false; }
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

double Game::generationRight() const
{
    return player.x + std::max(static_cast<double>(GenerationAhead), viewWidth - CatScreenX + PlatformWidth);
}

void Game::generatePlatforms()
{
    for (std::size_t index = 0; index < platforms.size(); ++index) {
        auto& slot = platforms[index];
        if (slot && slot->x < player.x - GenerationAhead && support != index) {
            slot.reset();
            if (ignoredPlatform == index) { ignoredPlatform.reset(); }
        }
    }
    if (lastX < generationRight()) { generate(); }
}

void Game::updateLeaves(double deltaTime)
{
    // Интеграл ветра сохраняет одинаковое смещение при разной частоте кадров.
    const double windDistance = 700 * (std::cos((time - deltaTime) / 7) - std::cos(time / 7));
    for (auto& leaf : leaves) {
        if (leaf.active && leaf.x >= player.x - 1050 && leaf.x <= player.x + 3000 && leaf.y <= 725) {
            leaf.x -= windDistance + deltaTime * leaf.speed * 0.25;
            leaf.y += deltaTime * leaf.speed;
            leaf.angle += deltaTime * leaf.spin;
        } else {
            leaf.active = false;
        }
    }
    for (auto& leaf : leaves) {
        if (leaf.active) { continue; }
        leaf.y = leafRandom() % 350 - 400;
        leaf.x = leafRandom() % 3770 + player.x - 1050;
        leaf.speed = (leafRandom() % 15 / 100.0 + 0.1) / 1.5 * 1000;
        leaf.angle = 0;
        leaf.spin = (leafRandom() % 4 - 1) * 50;
        leaf.active = true;
    }
}

bool Game::supportsPlayer(const Platform& platform, double playerX) const
{
    const double point = playerX + SupportPointOffset;
    return point >= platform.x && point <= platform.x + PlatformWidth;
}

void Game::respawn()
{
    support.reset();
    ignoredPlatform.reset();
    player.y = -FeetOffset;
    player.velocityY = RespawnFallSpeed;
    fallDirection = 0;
    player.grounded = false;
    jumpBufferTime = 0;
}

void Game::updatePlayer(double deltaTime, const Controls& controls)
{
    double remaining = deltaTime;
    while (remaining > TimeTolerance) {
        if (player.y >= CanvasHeight) { respawn(); }
        double duration = remaining;
        for (auto& slot : platforms) {
            if (!slot || slot->art != Art::MovingPlatform || slot->speed <= 0) { continue; }
            auto& platform = *slot;
            if (platform.bottom <= platform.top) { continue; }
            if (platform.y <= platform.top + ContactTolerance) {
                platform.y = platform.top;
                platform.movingDown = true;
            } else if (platform.y >= platform.bottom - ContactTolerance) {
                platform.y = platform.bottom;
                platform.movingDown = false;
            }
            const double distance = platform.movingDown ? platform.bottom - platform.y : platform.y - platform.top;
            duration = std::min(duration, distance / platform.speed);
        }
        if (support && (!platforms[*support] || !supportsPlayer(*platforms[*support], player.x))) {
            if (platforms[*support]) {
                fallDirection = player.x + SupportPointOffset > platforms[*support]->x + PlatformWidth / 2.0 ? 1 : -1;
            }
            support.reset();
        }
        player.grounded = support.has_value();
        if (dropRequested && support) {
            ignoredPlatform = support;
            const auto& platform = *platforms[*support];
            const double platformSpeed = platformVelocityY(platform);
            player.velocityY = std::max(DropSpeed, platformSpeed + DropSpeed);
            support.reset();
            player.grounded = false;
            jumpBufferTime = 0;
            fallDirection = 0;
        } else if (jumpBufferTime > 0 && support) {
            player.velocityY = JumpSpeed + platformVelocityY(*platforms[*support]);
            support.reset();
            player.grounded = false;
            jumpBufferTime = 0;
            fallDirection = 0;
        }
        dropRequested = false;

        const int direction = static_cast<int>(controls.right) - static_cast<int>(controls.left);
        const double targetSpeed = direction * MaximumWalkSpeed;
        const double acceleration = direction == 0
            ? (player.grounded ? GroundBraking : AirBraking)
            : (player.grounded ? GroundAcceleration : AirAcceleration);
        const double difference = targetSpeed - player.velocityX;
        const double accelerationX = std::abs(difference) < ContactTolerance ? 0
            : std::copysign(acceleration, difference);
        const double targetTime = std::abs(difference) / acceleration;
        if (accelerationX != 0) {
            duration = std::min(duration, targetTime);
            // При смене направления траектория каждого участка остаётся монотонной по X.
            if (player.velocityX * accelerationX < 0) {
                duration = std::min(duration, -player.velocityX / accelerationX);
            }
        }

        bool leavingEdge = false;
        double edgeX = 0;
        if (support) {
            const auto& platform = *platforms[*support];
            const double nextX = player.x + player.velocityX * duration + accelerationX * duration * duration / 2;
            if (!supportsPlayer(platform, nextX)) {
                edgeX = nextX > player.x ? platform.x + PlatformWidth - SupportPointOffset
                    : platform.x - SupportPointOffset;
                const double distance = edgeX - player.x;
                double edgeTime = 0;
                if (accelerationX == 0) {
                    edgeTime = distance / player.velocityX;
                } else {
                    const double discriminant = std::max(0.0, player.velocityX * player.velocityX
                        + 2 * accelerationX * distance);
                    const double root = std::sqrt(discriminant);
                    const double first = (-player.velocityX + root) / accelerationX;
                    const double second = (-player.velocityX - root) / accelerationX;
                    edgeTime = first >= -TimeTolerance ? first : second;
                    if (second >= -TimeTolerance) { edgeTime = std::min(edgeTime, second); }
                }
                duration = std::clamp(edgeTime, 0.0, duration);
                leavingEdge = true;
            }
        }

        std::optional<std::size_t> landing;
        bool returningToTop = false;
        const double slideSpeed = support ? 0 : fallDirection * EdgeSlideSpeed;
        const double horizontalSpeed = player.velocityX + slideSpeed;
        if (!support) {
            const double feet = player.y + FeetOffset;
            for (std::size_t index = 0; index < platforms.size(); ++index) {
                const auto& slot = platforms[index];
                if (!slot || ignoredPlatform == index) { continue; }
                const double gap = feet - slot->y;
                if (gap > ContactTolerance) { continue; }
                const double platformSpeed = platformVelocityY(*slot);
                const double relativeSpeed = player.velocityY - platformSpeed;
                const double root = std::sqrt(std::max(0.0, relativeSpeed * relativeSpeed - 2 * Gravity * gap));
                const double contact = relativeSpeed >= 0 && root + relativeSpeed > 0
                    ? -2 * gap / (root + relativeSpeed) : (root - relativeSpeed) / Gravity;
                if (contact < -TimeTolerance || contact > duration + TimeTolerance) { continue; }
                const double contactX = player.x + horizontalSpeed * contact + accelerationX * contact * contact / 2;
                if (supportsPlayer(*slot, contactX) && (!landing || contact < duration - TimeTolerance)) {
                    duration = std::max(0.0, contact);
                    landing = index;
                }
            }
            const double distanceToBottom = CanvasHeight - player.y;
            const double root = std::sqrt(player.velocityY * player.velocityY + 2 * Gravity * distanceToBottom);
            const double exitTime = player.velocityY >= 0
                ? 2 * distanceToBottom / (root + player.velocityY) : (root - player.velocityY) / Gravity;
            if (exitTime <= duration && (!landing || exitTime < duration - TimeTolerance)) {
                duration = exitTime;
                landing.reset();
                returningToTop = true;
            }
        }

        // dT делится по событиям: контакт, разворот платформы, смена режима движения и выход за экран.
        player.x += horizontalSpeed * duration + accelerationX * duration * duration / 2;
        // Камера не компенсирует боковое сползание; после него плавно возвращает кота к опорной позиции экрана.
        if (fallDirection != 0) {
            cameraOffsetX += slideSpeed * duration;
        } else {
            cameraOffsetX *= std::exp(-CameraReturnRate * duration);
        }
        player.velocityX += accelerationX * duration;
        if (accelerationX != 0 && std::abs(duration - targetTime) <= TimeTolerance) {
            player.velocityX = targetSpeed;
        }
        if (!support) {
            player.y += player.velocityY * duration + Gravity * duration * duration / 2;
            player.velocityY += Gravity * duration;
        }
        for (auto& slot : platforms) {
            if (!slot || slot->art != Art::MovingPlatform || slot->bottom <= slot->top) { continue; }
            slot->y += (slot->movingDown ? slot->speed : -slot->speed) * duration;
        }
        if (support) {
            player.y = platforms[*support]->y - FeetOffset;
            player.velocityY = 0;
            fallDirection = 0;
        }
        if (landing) {
            support = landing;
            player.y = platforms[*landing]->y - FeetOffset;
            player.velocityY = 0;
            fallDirection = 0;
        }
        if (leavingEdge) {
            const auto& platform = *platforms[*support];
            player.x = edgeX;
            player.velocityY = platformVelocityY(platform);
            fallDirection = edgeX + SupportPointOffset > platform.x + PlatformWidth / 2.0 ? 1 : -1;
            // Покинутая платформа не захватывает кота повторно в момент пересечения края.
            ignoredPlatform = support;
            support.reset();
        }
        if (returningToTop) { respawn(); }
        player.grounded = support.has_value();
        remaining -= duration;
        jumpBufferTime = std::max(0.0, jumpBufferTime - duration);
    }
    if (ignoredPlatform && (!platforms[*ignoredPlatform]
        || player.y + FeetOffset > platforms[*ignoredPlatform]->y + PlatformHeight)) {
        ignoredPlatform.reset();
    }
}

void Game::updateAnimation(const Controls& controls)
{
    const int direction = static_cast<int>(controls.right) - static_cast<int>(controls.left);
    if (direction == 0) {
        headTime = time;
        feetTime = time;
        return;
    }
    const int headSteps = static_cast<int>((time - headTime) / AnimationInterval);
    player.head = std::clamp(player.head + direction * headSteps, 1, 5);
    headTime += headSteps * AnimationInterval;
    const int feetSteps = static_cast<int>((time - feetTime) / AnimationInterval);
    player.feet = ((player.feet - 1 + direction * (feetSteps % 12)) % 12 + 12) % 12 + 1;
    feetTime += feetSteps * AnimationInterval;
}

void Game::update(double deltaTime, const Controls& controls)
{
    if (controls.up && !jumpHeld) { jumpBufferTime = JumpBufferDuration; }
    if (controls.down && !downHeld) { dropRequested = true; }
    jumpHeld = controls.up;
    downHeld = controls.down;
    generatePlatforms();
    updatePlayer(deltaTime, controls);
    time += deltaTime;
    updateLeaves(deltaTime);
    updateAnimation(controls);
    cameraX = player.x - CatScreenX - cameraOffsetX;
}
}
