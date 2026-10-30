#include "game.h"
#include "geometry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace Cat
{
namespace
{
constexpr std::size_t PlatformLimit = 50;
}

Game::Game(std::uint32_t routeSeed, std::uint32_t leavesSeed)
    : routeRandom(routeSeed)
    , leavesRandom(leavesSeed)
{
    addPlatform(lastX, lastY, Stone);
    for (auto& leaf : leaves) {
        constexpr int LeafKindChoices = 8;
        const int type = random(leavesRandom) % LeafKindChoices;
        leaf.art = type == LeafKindChoices - 1 ? Leaf1 : (type % 3 == 0 ? Leaf2 : Leaf3);
    }
}

int Game::random(std::uint32_t& state)
{
    // Линейный конгруэнтный генератор MSVCRT rand().
    state = state * 214013U + 2531011U;
    return static_cast<int>((state >> 16U) & 32767U);
}

void Game::addPlatform(int x, int y, Art art, double top, double bottom)
{
    Platform platform{nextId++, Engine::Vector2{static_cast<float>(x), static_cast<float>(y)}, art,
        std::vector<Decoration>{}, 0, 0, 0};
    if (art == Lift) {
        const double catX = cat.position().x - 101.25;
        platform.speed = -std::min(400.0,
            200.0 + (1 - std::pow(5.0, -catX / 100000.0)) * 200.0);
        platform.top = top;
        platform.bottom = bottom;
        platform.position.y = static_cast<float>(std::clamp(static_cast<double>(platform.position.y), top, bottom));
    }
    platforms.push_back(std::move(platform));
}

void Game::decorate(Platform& platform)
{
    const auto add = [&](Art art, int x) {
        const int height = art == Mushroom ? 58 : (art == Flowers ? 80 : 35);
        platform.decorations.push_back(Decoration{art, Engine::Vector2{static_cast<float>(x), static_cast<float>(2 - height)}});
    };
    constexpr int FlowersScatter = 16;
    constexpr int DecorationPairSpacing = 88;
    constexpr int DecorationPatterns = 8;
    switch (random(routeRandom) % DecorationPatterns) {
    case 0: {
        constexpr int MixedPatterns = 5;
        switch (random(routeRandom) % MixedPatterns + 1) {
        case 1: {
            add(Mushroom, random(routeRandom) % 52 + 31);
            break;
        }
        case 2: {
            const int first = random(routeRandom) % FlowersScatter;
            add(Flowers, first);
            add(Flowers, random(routeRandom) % FlowersScatter + DecorationPairSpacing);
            break;
        }
        case 5: {
            constexpr int MixedScatter = 30;
            int mushroomX;
            int flowersX;
            if (random(routeRandom) % 2 == 0) {
                mushroomX = random(routeRandom) % MixedScatter;
                flowersX = random(routeRandom) % (70 - mushroomX) + mushroomX + 64;
            } else {
                flowersX = random(routeRandom) % MixedScatter;
                mushroomX = random(routeRandom) % (60 - flowersX) + flowersX + 75;
            }
            add(Mushroom, mushroomX - 10);
            add(Flowers, flowersX);
            break;
        }
        default: break;
        }
        break;
    }
    case 1: case 2: case 3: {
        add(Flowers, 52);
        break;
    }
    case 4: case 5: {
        add(Grass, random(routeRandom) % 135);
        break;
    }
    case 6: {
        add(Flowers, random(routeRandom) % FlowersScatter + 36);
        break;
    }
    case 7: {
        constexpr int GrassPairScatter = 48;
        const int first = random(routeRandom) % GrassPairScatter;
        add(Grass, first);
        add(Grass, random(routeRandom) % GrassPairScatter + DecorationPairSpacing);
        break;
    }
    }
}

bool Game::overlaps(int x, int y) const
{
    const Engine::Rect candidate{static_cast<float>(x), static_cast<float>(y), 175, 35};
    return std::any_of(platforms.begin(), platforms.end(), [&](const Platform& platform) {
        const Engine::Rect bounds{static_cast<float>(static_cast<int>(platform.position.x)),
            static_cast<float>(static_cast<int>(platform.position.y)), 175, 35};
        Engine::Rect overlap;
        // Касание краями допустимо при генерации площадок.
        return Engine::intersection(candidate, bounds, overlap)
            && overlap.size.width > 0 && overlap.size.height > 0;
    });
}

void Game::generateNormal(double catX, double horizon)
{
    const bool variedHeights = random(routeRandom) % 2 == 0;
    // Расстояния возрастают с пройденным путём; радиус достижимости — 200 пикселей.
    const int gap = static_cast<int>((1 - std::pow(25.0, -catX / 100000.0)) * 275 + 175);
    const int span = catX > 100000.0 ? 460
        : static_cast<int>(gap + 40 - 30 * catX / 100000.0);
    while (lastX < horizon && platforms.size() < PlatformLimit) {
        const int y = variedHeights ? random(routeRandom) % 500 + 150
            : (random(routeRandom) % 100) * 5 + 150;
        const int offset = random(routeRandom) % span;
        const int x = lastX + random(routeRandom) % 200 + offset;
        const int dy = y - lastY;
        const int distance = lastX - x + span;
        const int shortDistance = lastX - x + gap;
        constexpr int MaximumHeightDifference = 195;
        constexpr int PreferredHeightDifference = 185;
        constexpr int ReachRadiusSquared = 200 * 200;
        if (std::abs(dy) >= MaximumHeightDifference
            || (x >= span && dy * dy + distance * distance >= ReachRadiusSquared && distance < 0)
            || (std::abs(dy) <= PreferredHeightDifference && (!variedHeights || distance > 0))
            || (x <= gap && dy * dy + shortDistance * shortDistance <= ReachRadiusSquared && shortDistance < 0)
            || overlaps(x, y)) {
            continue;
        }
        const bool plain = random(routeRandom) % 2 == 0;
        addPlatform(x, y, plain ? Stone : Turf);
        if (!plain) {
            decorate(platforms.back());
        }
        lastX = x;
        lastY = y;
    }
}

bool Game::generateSpecial(double catX, bool zigzag)
{
    constexpr int ZigzagLifts = 4;
    const std::size_t sectionSize = zigzag ? ZigzagLifts + 2U : 3U;
    if (platforms.size() + sectionSize > PlatformLimit) {
        return false;
    }
    constexpr int RestHeight = 300;
    if (!zigzag) {
        const int bottom = std::min(lastY + 400, 650);
        const int x = lastX + random(routeRandom) % 50 + 250;
        addPlatform(x, bottom, Stone);
        const int top = random(routeRandom) % 200 + 150;
        constexpr int LiftSectionSpacing = 200;
        addPlatform(x + LiftSectionSpacing, RestHeight, Lift, top, bottom);
        lastX = x + 2 * LiftSectionSpacing;
        lastY = top;
    } else {
        const double stretch = std::min(1.0, 1 - std::pow(5.0, -catX / 100000.0));
        constexpr int ZigzagSpacing = 320;
        int middleX = 0;
        for (int index = 0; index < ZigzagLifts; ++index) {
            constexpr int MiddleLift = 2;
            int x = static_cast<int>(lastX + ZigzagSpacing + 200 * stretch);
            if (index == MiddleLift) {
                middleX = x;
                x = static_cast<int>(lastX + 2 * ZigzagSpacing + 2 * 200 * stretch);
            }
            lastY = index % 2 ? 650 : 150;
            addPlatform(x, lastY, Lift, 150, static_cast<int>(650 - 100 * stretch));
            lastX = x;
        }
        addPlatform(middleX, RestHeight, Turf);
        lastX += ZigzagSpacing;
        lastY = RestHeight;
    }
    addPlatform(lastX, lastY, Stone);
    return true;
}

void Game::generate(double catX, double horizon)
{
    if (lastX >= horizon) {
        return;
    }
    constexpr int ZigzagDominantFrom = 20000;
    constexpr int ZigzagDominantUntil = 30000;
    if (catX > nextPatternX && catX > 3000 && catX != ZigzagDominantFrom && catX != ZigzagDominantUntil) {
        const bool zigzagDominant = catX > ZigzagDominantFrom && catX < ZigzagDominantUntil;
        const int choice = random(routeRandom) % 3;
        const bool zigzag = zigzagDominant ? choice != 0 : choice == 0;
        pattern = zigzag ? Pattern::Zigzag : Pattern::Lift;
        nextPatternX += random(routeRandom) % 1000 + (zigzag ? 3000 : 1500);
    }
    if (pattern == Pattern::Normal) {
        generateNormal(catX, horizon);
    } else if (generateSpecial(catX, pattern == Pattern::Zigzag)) {
        pattern = Pattern::Normal;
    }
}

void Game::updateLeaves(double duration, double catX)
{
    time += duration;
    // Интеграл ветра сохраняет одинаковый снос при разной частоте кадров.
    const double wind = 700.0 * (std::cos((time - duration) / 7.0) - std::cos(time / 7.0));
    for (auto& leaf : leaves) {
        constexpr int LeafRangeBehind = 1050;
        constexpr int LeafRangeAhead = 3000;
        if (leaf.active && leaf.position.x >= catX - LeafRangeBehind && leaf.position.x <= catX + LeafRangeAhead
            && leaf.position.y <= 700 + 25.0) {
            leaf.position.x -= static_cast<float>(wind + duration * leaf.speed * 0.25);
            leaf.position.y += static_cast<float>(duration * leaf.speed);
            leaf.angle += duration * leaf.spin;
        } else {
            leaf.position.y = static_cast<float>(random(leavesRandom) % 350 - 400);
            leaf.position.x = static_cast<float>(random(leavesRandom) % 3770 + catX - LeafRangeBehind);
            leaf.speed = (random(leavesRandom) % 15 / 100.0 + 0.1)
                / 1.5 * 1000;
            leaf.angle = 0;
            leaf.spin = (random(leavesRandom) % 4 - 1) * 50;
            leaf.active = true;
        }
    }
}

void Game::update(double duration, const Controls& controls)
{
    const double catX = cat.position().x - 101.25;
    const double camera = cat.position().x - 450.0;
    constexpr double GenerationDistance = 1024;
    const double horizon = std::max(catX + GenerationDistance, camera + visibleWidth + 175);
    platforms.erase(std::remove_if(platforms.begin(), platforms.end(), [&](const Platform& platform) {
        return platform.position.x < catX - GenerationDistance && !cat.standsOn(platform);
    }), platforms.end());
    generate(catX, horizon);
    cat.advance(platforms, duration, controls);
    updateLeaves(duration, cat.position().x - 101.25);
}
}
