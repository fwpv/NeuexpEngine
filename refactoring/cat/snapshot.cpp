#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <set>
#include <vector>
#include "game.h"

namespace
{
void vector(Engine::Vector2 value)
{
    std::cout << ' ' << value.x << ' ' << value.y;
}

void snapshot(Cat::Game& game)
{
#ifdef REFACTORED
    auto& world = game.world;
    auto& foliage = game.foliage;
#else
    auto& world = game;
    auto& foliage = game;
#endif
    const auto& cat = world.cat;
    vector(cat.feet);
    vector(cat.velocity);
    std::cout << ' ' << cat.support << ' ' << cat.entryDirection << ' ' << cat.jumpBuffer
        << ' ' << cat.dropRequested << ' ' << cat.headFrame << ' ' << cat.feetFrame
        << ' ' << cat.animationTime << ' ' << cat.frameTime << ' ' << cat.ignored.size();
    for (int id : cat.ignored) {
        std::cout << ' ' << id;
    }
    std::cout << ' ' << world.routeRandom << ' ' << world.nextId << ' ' << static_cast<int>(world.pattern)
        << ' ' << world.nextPatternX << ' ' << world.lastX << ' ' << world.lastY
        << ' ' << foliage.leavesRandom << ' ' << foliage.time << ' ' << game.visibleWidth;
    std::cout << ' ' << world.platforms.size();
    for (const auto& platform : world.platforms) {
        std::cout << ' ' << platform.id << ' ' << platform.art;
        vector(platform.position);
        std::cout << ' ' << platform.speed << ' ' << platform.top << ' ' << platform.bottom
            << ' ' << platform.decorations.size();
        for (const auto& decoration : platform.decorations) {
            std::cout << ' ' << decoration.art;
            vector(decoration.offset);
        }
    }
    for (const auto& leaf : foliage.leaves) {
        std::cout << ' ' << leaf.art;
        vector(leaf.position);
        std::cout << ' ' << leaf.angle << ' ' << leaf.speed << ' ' << leaf.spin << ' ' << leaf.active;
    }
    const auto& view = game.view;
    std::cout << ' ' << view.viewport.x << ' ' << view.viewport.y << ' ' << view.viewport.width << ' ' << view.viewport.height;
    vector(view.worldBounds.position);
    std::cout << ' ' << view.worldBounds.size.width << ' ' << view.worldBounds.size.height;
    for (const auto& transform : {view.worldToScreenTransform, view.screenToWorldTransform}) {
        std::cout << ' ' << transform.m00 << ' ' << transform.m01 << ' ' << transform.m02
            << ' ' << transform.m10 << ' ' << transform.m11 << ' ' << transform.m12;
    }
    std::cout << '\n';
}
}

int main()
{
    std::cout << std::hexfloat;
    for (std::uint32_t seed : {0U, 1U, 42U, 32767U, 0xffffffffU}) {
        for (int scenario = 0; scenario < 5; ++scenario) {
            Cat::Game game(seed, seed ^ 0x12345678U);
            snapshot(game);
            for (int frame = 0; frame < 1800; ++frame) {
#ifdef REFACTORED
                auto& world = game.world;
#else
                auto& world = game;
#endif
                // Диагностические состояния покрывают границы выбора маршрута и дальние координаты.
                if (scenario == 4 && frame % 100 == 0) {
                    constexpr std::array<float, 9> Positions{3000, 3000.25F, 20000, 20000.25F,
                        30000, 30000.25F, 100000, 150000, -1000};
                    world.cat.feet.x = Positions[static_cast<std::size_t>(frame / 100) % Positions.size()] + 101.25F;
                }
                const Cat::Controls controls{scenario != 0 && (scenario != 3 || frame % 240 < 130),
                    scenario == 3 && frame % 240 >= 120,
                    scenario >= 2 && frame % 43 == 0, scenario >= 2 && frame % 137 == 0};
                constexpr std::array<float, 6> Durations{0, 0.001F, 0.01F, 1.0F / 60, 0.1F, 0.2F};
                game.update(Durations[static_cast<std::size_t>(frame) % Durations.size()], controls);
                Engine::CameraError error;
                const int width = frame % 3 == 0 ? 1920 : 935;
                const int height = frame % 3 == 1 ? 1080 : 700;
                if (!game.prepareView(width, height, error)) {
                    return 1;
                }
                snapshot(game);
            }
        }
    }
}
