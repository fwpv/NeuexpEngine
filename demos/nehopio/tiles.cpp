#include "tiles.h"

namespace Nehopio
{
namespace
{
// Пустое место первым: этот вид получают неизвестные символы.
const std::vector<TileInfo> Tiles{
    {'.', false, nullptr, TileBump::None, '.'},
    {'#', true, "ground", TileBump::None, '#'},
    {'=', true, "soil", TileBump::None, '='},
    {'B', true, "brick", TileBump::Break, '.'},
    {'Q', true, "question", TileBump::Coin, 'U'},
    {'S', true, "stone", TileBump::None, 'S'},
    {'U', true, "used", TileBump::None, 'U'},
    {'M', true, "question", TileBump::Mushroom, 'U'},
};
}

const std::vector<TileInfo>& tileTypes()
{
    return Tiles;
}

const TileInfo& tileInfo(char symbol)
{
    for (const TileInfo& tile : Tiles) {
        if (tile.symbol == symbol) {
            return tile;
        }
    }
    return Tiles.front();
}

bool isTileSymbol(char symbol)
{
    for (const TileInfo& tile : Tiles) {
        if (tile.symbol == symbol) {
            return true;
        }
    }
    return false;
}

bool isSolid(char symbol)
{
    return tileInfo(symbol).solid;
}
}
