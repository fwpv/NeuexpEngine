#pragma once

#include <vector>

namespace Nehopio
{
// Что происходит с плиткой, когда игрок бьёт её головой снизу.
enum class TileBump
{
    None,
    // Выпускает монету и становится использованной.
    Coin,
    // Разрушается и исчезает.
    Break,
    // Выпускает гриб и становится использованной.
    Mushroom
};

// Вид плитки уровня: символ карты, твёрдость, участок листа плиток и последствия удара.
struct TileInfo
{
    char symbol;
    bool solid;
    // Имя участка атласа плиток; nullptr — плитка не рисуется (пустое место).
    const char* region;
    TileBump bump;
    // Символ плитки после удара головой; для плиток без последствий совпадает с symbol.
    char bumpInto;
};

// Виды плиток уровня: единственное место, где описан «язык» карты.
const std::vector<TileInfo>& tileTypes();
// Вид плитки по символу; неизвестный символ ведёт себя как пустое место.
const TileInfo& tileInfo(char symbol);
// Символ описан как плитка (включая пустое место).
bool isTileSymbol(char symbol);
// Твёрдость символа: по ней решаются столкновения игрока, врагов и предметов.
bool isSolid(char symbol);
}
