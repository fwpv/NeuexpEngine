#pragma once

#include "core/geometry.h"
#include "core/math2d.h"
#include "core/resources.h"

#include <string>
#include <vector>

namespace Nehopio
{
// Диапазон клеток уровня; концы включены.
struct TileRange
{
    int firstColumn = 0;
    int lastColumn = -1;
    int firstRow = 0;
    int lastRow = -1;
};

// Уровень читается из текстового файла: строки равной длины, одна плитка — один символ.
// Плитки и их твёрдость описаны в tiles.h, остальные символы размещают объекты
// и в столкновениях не участвуют.
class Level
{
public:
    static constexpr float TileSize = 32;

    bool load(const NeuexpEngine::ResourceFiles& files, const std::string& name, NeuexpEngine::ResourceError& error);
    // Возвращает плитки к состоянию после загрузки: сломанные и использованные
    // блоки снова становятся исходными.
    void restore();

    int width() const;
    int height() const;
    float pixelWidth() const;
    float pixelHeight() const;

    // Клетки, пересекающие прямоугольную область мира: остальные рисовать не нужно.
    TileRange tileRange(const NeuexpEngine::Rect& area) const;

    char tile(int column, int row) const;
    bool setTile(int column, int row, char value);
    bool solid(int column, int row) const;
    bool solidAt(NeuexpEngine::Vector2 point) const;
    // Касание правой и нижней границей плитки не считается пересечением, поэтому
    // тело, стоящее на поверхности, не считается столкнувшимся с плиткой под ногами.
    bool overlapsSolid(const NeuexpEngine::Rect& box) const;

    // Позиция ног игрока: нижняя граница клетки спавна.
    NeuexpEngine::Vector2 playerSpawn() const;
    const std::vector<NeuexpEngine::Vector2>& coinSpawns() const;
    const std::vector<NeuexpEngine::Vector2>& enemySpawns() const;
    const std::vector<NeuexpEngine::Vector2>& flyerSpawns() const;
    NeuexpEngine::Rect goal() const;

private:
    // Чтение строк уровня и проверка формата: непустой, равная ширина, известные символы.
    bool readRows(const std::string& name, const std::string& text, std::vector<std::string>& rows,
        NeuexpEngine::ResourceError& error) const;

    std::vector<std::string> rows;
    // Сетка сразу после загрузки: к ней возвращает restore().
    std::vector<std::string> initialRows;
    NeuexpEngine::Vector2 spawn;
    std::vector<NeuexpEngine::Vector2> coins;
    std::vector<NeuexpEngine::Vector2> enemies;
    std::vector<NeuexpEngine::Vector2> flyers;
    NeuexpEngine::Rect goalBounds;
};
}
