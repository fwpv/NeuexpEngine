#include "world.h"
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

int random(std::uint32_t& state)
{
    // Линейный конгруэнтный генератор MSVCRT rand().
    state = state * 214013U + 2531011U;
    return static_cast<int>((state >> 16U) & 32767U);
}
}

Platform::Platform(int id, Engine::Vector2 position, Art art)
    : id(id)
    , position(position)
    , size(DefaultSize)
    , art(art)
{
}

World::World(std::uint32_t seed)
    : routeRandom(seed)
{
    addPlatform(lastX, lastY, Stone);
}

void World::addPlatform(int x, int y, Art art, float top, float bottom)
{
    Platform platform{nextId++, Engine::Vector2{static_cast<float>(x), static_cast<float>(y)}, art};
    if (art == Lift) {
        const float catX = cat.position().x - 101.25F;
        platform.speed = -std::min(400.0F,
            200.0F + (1 - std::pow(5.0F, -catX / 100000.0F)) * 200.0F);
        platform.top = top;
        platform.bottom = bottom;
        platform.position.y = std::clamp(platform.position.y, top, bottom);
    }
    platforms.push_back(std::move(platform));
}

void World::decorate(Platform& platform)
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

bool World::overlaps(int x, int y) const
{
    const Engine::Rect candidate{Engine::Vector2{static_cast<float>(x), static_cast<float>(y)}, Platform::DefaultSize};
    return std::any_of(platforms.begin(), platforms.end(), [&](const Platform& platform) {
        const Engine::Rect bounds{Engine::Vector2{static_cast<float>(static_cast<int>(platform.position.x)),
            static_cast<float>(static_cast<int>(platform.position.y))}, platform.size};
        Engine::Rect overlap;
        // Касание краями допустимо при генерации площадок.
        return Engine::intersection(candidate, bounds, overlap)
            && overlap.size.width > 0 && overlap.size.height > 0;
    });
}

void World::generateNormal(float catX, float horizon)
{
    const bool variedHeights = random(routeRandom) % 2 == 0;
    // Расстояния возрастают с пройденным путём; радиус достижимости — 200 пикселей.
    const int gap = static_cast<int>((1 - std::pow(25.0F, -catX / 100000.0F)) * 275 + Platform::DefaultSize.width);
    const int span = catX > 100000.0F ? 460
        : static_cast<int>(static_cast<float>(gap + 40) - 30 * catX / 100000.0F);
    while (static_cast<float>(lastX) < horizon && platforms.size() < PlatformLimit) {
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

bool World::generateSpecial(float catX, bool zigzag)
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
        addPlatform(x + LiftSectionSpacing, RestHeight, Lift, static_cast<float>(top), static_cast<float>(bottom));
        lastX = x + 2 * LiftSectionSpacing;
        lastY = top;
    } else {
        const float stretch = std::min(1.0F, 1 - std::pow(5.0F, -catX / 100000.0F));
        constexpr int ZigzagSpacing = 320;
        int middleX = 0;
        for (int index = 0; index < ZigzagLifts; ++index) {
            constexpr int MiddleLift = 2;
            int x = static_cast<int>(static_cast<float>(lastX + ZigzagSpacing) + 200 * stretch);
            if (index == MiddleLift) {
                middleX = x;
                x = static_cast<int>(static_cast<float>(lastX + 2 * ZigzagSpacing) + 2 * 200 * stretch);
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

void World::generate(float catX, float horizon)
{
    if (static_cast<float>(lastX) >= horizon) {
        return;
    }
    constexpr int ZigzagDominantFrom = 20000;
    constexpr int ZigzagDominantUntil = 30000;
    if (catX > static_cast<float>(nextPatternX) && catX > 3000 && catX != ZigzagDominantFrom && catX != ZigzagDominantUntil) {
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

Engine::Vector2 World::catPosition() const
{
    return cat.position();
}

void World::update(float duration, const Controls& controls, float visibleWidth)
{
    const float catX = cat.position().x - 101.25F;
    const float camera = cat.position().x - 450.0F;
    constexpr float GenerationDistance = 1024;
    const float horizon = std::max(catX + GenerationDistance, camera + visibleWidth + Platform::DefaultSize.width);
    platforms.erase(std::remove_if(platforms.begin(), platforms.end(), [&](const Platform& platform) {
        return platform.position.x < catX - GenerationDistance && !cat.standsOn(platform);
    }), platforms.end());
    generate(catX, horizon);
    cat.advance(platforms, duration, controls);
}

Foliage::Foliage(std::uint32_t seed)
    : leavesRandom(seed)
{
    for (auto& leaf : leaves) {
        constexpr int LeafKindChoices = 8;
        const int type = random(leavesRandom) % LeafKindChoices;
        leaf.art = type == LeafKindChoices - 1 ? Leaf1 : (type % 3 == 0 ? Leaf2 : Leaf3);
    }
}

void Foliage::update(float duration, float catX)
{
    time += duration;
    // Интеграл ветра сохраняет одинаковый снос при разной частоте кадров.
    const float wind = 700.0F * (std::cos((time - duration) / 7.0F) - std::cos(time / 7.0F));
    for (auto& leaf : leaves) {
        constexpr int LeafRangeBehind = 1050;
        constexpr int LeafRangeAhead = 3000;
        if (leaf.active && leaf.position.x >= catX - LeafRangeBehind && leaf.position.x <= catX + LeafRangeAhead
            && leaf.position.y <= 700 + 25.0F) {
            leaf.position.x -= wind + duration * leaf.speed * 0.25F;
            leaf.position.y += duration * leaf.speed;
            leaf.angle += duration * leaf.spin;
        } else {
            leaf.position.y = static_cast<float>(random(leavesRandom) % 350 - 400);
            leaf.position.x = static_cast<float>(random(leavesRandom) % 3770) + catX - LeafRangeBehind;
            leaf.speed = (static_cast<float>(random(leavesRandom) % 15) / 100.0F + 0.1F)
                / 1.5F * 1000;
            leaf.angle = 0;
            leaf.spin = static_cast<float>((random(leavesRandom) % 4 - 1) * 50);
            leaf.active = true;
        }
    }
}

float Platform::prepareMotion(float duration)
{
    constexpr float TurnTolerance = 0.01F;
    if (speed == 0) {
        return duration;
    }
    if (position.y <= top + TurnTolerance) {
        position.y = top;
        speed = std::abs(speed);
    } else if (position.y >= bottom - TurnTolerance) {
        position.y = bottom;
        speed = -std::abs(speed);
    }
    return std::min(duration, (speed > 0 ? bottom - position.y : position.y - top) / std::abs(speed));
}

void Platform::draw(Engine::Renderer2D& renderer, const Images& images, const Engine::Rect& visible) const
{
    constexpr float PlatformCullBehind = 250;
    constexpr float PlatformCullAhead = 5;
    if (position.x <= visible.position.x - PlatformCullBehind
        || position.x >= visible.position.x + visible.size.width + PlatformCullAhead) {
        return;
    }
    images.draw(renderer, art, position);
    for (const auto& decoration : decorations) {
        images.draw(renderer, decoration.art, position + decoration.offset);
    }
}

void Leaf::draw(Engine::Renderer2D& renderer, const Images& images, const Engine::Rect& visible) const
{
    if (active && Engine::intersects(visible, Engine::Rect{position - Engine::Vector2{25, 25}, Engine::Size2{50, 50}})) {
        images.draw(renderer, art, position, 1, angle);
    }
}

void Foliage::draw(Engine::Renderer2D& renderer, const Images& images, const Engine::Rect& visible) const
{
    for (const auto& leaf : leaves) {
        leaf.draw(renderer, images, visible);
    }
}

void World::draw(Engine::Renderer2D& renderer, const Images& images, const Engine::Rect& visible) const
{
    for (const auto& platform : platforms) {
        platform.draw(renderer, images, visible);
    }
    cat.draw(renderer, images);
}

}
