#include "level.h"

#include "geometry.h"
#include "tiles.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace Nehopio
{
namespace
{
// Символы объектов уровня: точки появления и финиша.
bool isObjectSymbol(char symbol)
{
    return symbol == 'P' || symbol == 'o' || symbol == 'E' || symbol == 'V' || symbol == 'F';
}

bool isKnownChar(char symbol)
{
    return isTileSymbol(symbol) || isObjectSymbol(symbol);
}

// 'F' задаёт основание флага; сама зона финиша поднята над землёй на высоту древка.
NeuexpEngine::Rect flagBounds(int column, int row)
{
    constexpr float PoleWidth = 20;
    constexpr float PoleHeight = 116;
    const float left = static_cast<float>(column) * Level::TileSize + (Level::TileSize - PoleWidth) / 2;
    const float bottom = static_cast<float>(row + 1) * Level::TileSize;
    return NeuexpEngine::Rect{left, bottom - PoleHeight, PoleWidth, PoleHeight};
}
}

bool Level::load(const NeuexpEngine::ResourceFiles& files, const std::string& name, NeuexpEngine::ResourceError& error)
{
    std::string text;
    if (!files.readText(name, text, error)) {
        return false;
    }
    std::vector<std::string> loaded;
    if (!readRows(name, text, loaded, error)) {
        return false;
    }

    // Разбор объектов: точки появления, монеты, враги и финиш. Результат применяется
    // к уровню только после проверки, что спавн и флаг найдены.
    const std::size_t width = loaded.front().size();
    NeuexpEngine::Vector2 loadedSpawn;
    std::vector<NeuexpEngine::Vector2> loadedCoins;
    std::vector<NeuexpEngine::Vector2> loadedEnemies;
    std::vector<NeuexpEngine::Vector2> loadedFlyers;
    NeuexpEngine::Rect loadedGoal;
    bool spawnFound = false;
    bool goalFound = false;
    for (std::size_t row = 0; row < loaded.size(); ++row) {
        for (std::size_t column = 0; column < width; ++column) {
            const float x = static_cast<float>(column) * TileSize;
            const float y = static_cast<float>(row) * TileSize;
            switch (loaded[row][column]) {
            case 'P':
                if (!spawnFound) {
                    // Ноги игрока ставим на нижнюю границу клетки спавна.
                    loadedSpawn = NeuexpEngine::Vector2{x + TileSize / 2, y + TileSize};
                    spawnFound = true;
                }
                break;
            case 'o':
                loadedCoins.push_back(NeuexpEngine::Vector2{x + TileSize / 2, y + TileSize / 2});
                break;
            case 'E':
                loadedEnemies.push_back(NeuexpEngine::Vector2{x + TileSize / 2, y + TileSize});
                break;
            case 'V':
                // Летающий враг висит по центру клетки.
                loadedFlyers.push_back(NeuexpEngine::Vector2{x + TileSize / 2, y + TileSize / 2});
                break;
            case 'F':
                if (!goalFound) {
                    loadedGoal = flagBounds(static_cast<int>(column), static_cast<int>(row));
                    goalFound = true;
                }
                break;
            default:
                break;
            }
        }
    }
    if (!spawnFound || !goalFound) {
        error = {NeuexpEngine::ResourceErrorCode::InvalidImage,
            std::string("Level requires 'P' and 'F': ") + name};
        return false;
    }

    initialRows = loaded;
    rows = std::move(loaded);
    spawn = loadedSpawn;
    coins = std::move(loadedCoins);
    enemies = std::move(loadedEnemies);
    flyers = std::move(loadedFlyers);
    goalBounds = loadedGoal;
    return true;
}

bool Level::readRows(const std::string& name, const std::string& text, std::vector<std::string>& rows,
    NeuexpEngine::ResourceError& error) const
{
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        rows.push_back(line);
    }
    while (!rows.empty() && rows.back().empty()) {
        rows.pop_back();
    }
    if (rows.empty()) {
        error = {NeuexpEngine::ResourceErrorCode::InvalidImage, "Level is empty: " + name};
        return false;
    }
    const std::size_t width = rows.front().size();
    if (width == 0) {
        error = {NeuexpEngine::ResourceErrorCode::InvalidImage, "Level has empty rows: " + name};
        return false;
    }
    for (std::size_t row = 0; row < rows.size(); ++row) {
        if (rows[row].size() != width) {
            error = {NeuexpEngine::ResourceErrorCode::InvalidImage,
                "Level rows have different width at row " + std::to_string(row)};
            return false;
        }
        for (std::size_t column = 0; column < width; ++column) {
            if (!isKnownChar(rows[row][column])) {
                error = {NeuexpEngine::ResourceErrorCode::InvalidImage,
                    "Unknown level symbol at " + std::to_string(column) + "," + std::to_string(row)};
                return false;
            }
        }
    }
    return true;
}

void Level::restore()
{
    rows = initialRows;
}

int Level::width() const
{
    return static_cast<int>(rows.empty() ? 0 : rows.front().size());
}

int Level::height() const
{
    return static_cast<int>(rows.size());
}

float Level::pixelWidth() const
{
    return static_cast<float>(width()) * TileSize;
}

float Level::pixelHeight() const
{
    return static_cast<float>(height()) * TileSize;
}

TileRange Level::tileRange(const NeuexpEngine::Rect& area) const
{
    // Правая и нижняя границы области исключены, как и у самих прямоугольников.
    const int firstColumn = static_cast<int>(std::floor(area.position.x / TileSize));
    const int lastColumn = static_cast<int>(std::floor((area.position.x + area.size.width) / TileSize));
    const int firstRow = static_cast<int>(std::floor(area.position.y / TileSize));
    const int lastRow = static_cast<int>(std::floor((area.position.y + area.size.height) / TileSize));
    return TileRange{std::max(0, firstColumn), std::min(width() - 1, lastColumn),
        std::max(0, firstRow), std::min(height() - 1, lastRow)};
}

char Level::tile(int column, int row) const
{
    if (column < 0 || row < 0 || column >= width() || row >= height()) {
        return '.';
    }
    return rows[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)];
}

bool Level::setTile(int column, int row, char value)
{
    if (column < 0 || row < 0 || column >= width() || row >= height()) {
        return false;
    }
    rows[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] = value;
    return true;
}

bool Level::solid(int column, int row) const
{
    return Nehopio::isSolid(tile(column, row));
}

bool Level::solidAt(NeuexpEngine::Vector2 point) const
{
    if (point.x < 0) {
        return false;
    }
    const int column = static_cast<int>(point.x / TileSize);
    const int row = static_cast<int>(point.y / TileSize);
    return solid(column, row);
}

bool Level::overlapsSolid(const NeuexpEngine::Rect& box) const
{
    constexpr float EdgeInset = 0.001F;
    const int firstColumn = static_cast<int>(std::floor(box.position.x / TileSize));
    const int lastColumn = static_cast<int>(std::floor((box.position.x + box.size.width - EdgeInset) / TileSize));
    const int firstRow = static_cast<int>(std::floor(box.position.y / TileSize));
    const int lastRow = static_cast<int>(std::floor((box.position.y + box.size.height - EdgeInset) / TileSize));
    for (int row = firstRow; row <= lastRow; ++row) {
        for (int column = firstColumn; column <= lastColumn; ++column) {
            if (solid(column, row)) {
                return true;
            }
        }
    }
    return false;
}

NeuexpEngine::Vector2 Level::playerSpawn() const
{
    return spawn;
}

const std::vector<NeuexpEngine::Vector2>& Level::coinSpawns() const
{
    return coins;
}

const std::vector<NeuexpEngine::Vector2>& Level::enemySpawns() const
{
    return enemies;
}

const std::vector<NeuexpEngine::Vector2>& Level::flyerSpawns() const
{
    return flyers;
}

NeuexpEngine::Rect Level::goal() const
{
    return goalBounds;
}
}
