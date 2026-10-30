#include "game.h"

namespace Cat
{
Game::Game(std::uint32_t routeSeed, std::uint32_t leavesSeed)
    : world(routeSeed)
    , foliage(leavesSeed)
{
}

void Game::update(float duration, const Controls& controls)
{
    // Листья используют позицию после движения; маршрут — до движения.
    world.update(duration, controls, visibleWidth);
    foliage.update(duration, world.catPosition().x - 101.25F);
}
}
