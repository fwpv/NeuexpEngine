#pragma once

#include "core/camera.h"
#include "core/math2d.h"
#include "graphics/renderer.h"
#include "physics/body.h"
#include "physics/world.h"
#include "scene/scene.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace PhysicsDemo
{
// Мир демонстрации: полоса подписей сверху и сетка зон проверки под ней.
constexpr float WorldWidth = 1100;
constexpr float WorldHeight = 700;
constexpr float CellWidth = 340;
constexpr float CellHeight = 300;
constexpr float GridLeft = 20;
constexpr float GridTop = 76;
constexpr float GridGapX = 20;
constexpr float GridGapY = 12;

// Зона проверки страницы: три столбца и два ряда.
NeuexpEngine::Rect cellArea(int column, int row);

// Одна проверка поведения физики. У зоны собственные сцена и физический мир,
// поэтому зоны не влияют друг на друга. Подпись зоны показывает наблюдение.
class Zone
{
public:
    Zone(NeuexpEngine::Rect area, std::string title);
    virtual ~Zone() = default;
    Zone(const Zone&) = delete;
    Zone& operator=(const Zone&) = delete;

    const NeuexpEngine::Rect& area() const { return zoneArea; }
    const std::string& title() const { return zoneTitle; }
    // Один шаг мира зоны постоянной длительности; dT конечен и положителен.
    void update(float deltaTime);
    // Текущее наблюдение зоны и счётчики для подписей и проверок.
    virtual std::string state() const = 0;
    virtual std::string counters() const { return {}; }
    bool highlighted() const { return highlight; }
    // Содержимое зоны: тела, сцена и подписи. Рисуется в пределах области зоны.
    void drawContent(NeuexpEngine::Renderer2D& drawing) const;
    // Рамка зоны рисуется последней, поверх содержимого соседних зон.
    void drawFrame(NeuexpEngine::Renderer2D& drawing) const;

protected:
    NeuexpEngine::Scene& scene() { return ownedScene; }
    const NeuexpEngine::Scene& scene() const { return ownedScene; }
    NeuexpEngine::PhysicsWorld& world() { return simulation; }
    const NeuexpEngine::PhysicsWorld& world() const { return simulation; }
    float elapsedSeconds() const { return elapsed; }
    void markHighlighted(bool value) { highlight = value; }

    // Действия зоны перед шагом и обработка событий шага.
    virtual void beforeStep(float deltaTime);
    virtual void afterStep();
    virtual void drawExtra(NeuexpEngine::Renderer2D& drawing) const;
    virtual NeuexpEngine::Color fillOf(const NeuexpEngine::RigidBody& body) const;
    virtual NeuexpEngine::Color outlineOf(const NeuexpEngine::RigidBody& body) const;

private:
    NeuexpEngine::Rect zoneArea;
    std::string zoneTitle;
    NeuexpEngine::Scene ownedScene;
    NeuexpEngine::PhysicsWorld simulation;
    float elapsed = 0;
    bool highlight = false;
};

// Фабрика проверок: страницы зон, их обновление, отрисовка и переключение.
class CheckBoard
{
public:
    CheckBoard() = default;
    CheckBoard(const CheckBoard&) = delete;
    CheckBoard& operator=(const CheckBoard&) = delete;

    // Строит все страницы заново и остаётся на текущей странице.
    void build();
    void restart() { build(); }

    void update(float deltaTime);
    // Содержимое каждой зоны обрезается по её области экрана; view даёт преобразование.
    void draw(NeuexpEngine::Renderer2D& drawing, const NeuexpEngine::CameraView& view) const;

    void nextPage();
    // Открывает страницу по индексу; индекс вне диапазона отклоняется assert-ом.
    void showPage(int page);
    int pageIndex() const { return currentPage; }
    int pageCount() const;
    const std::string& pageName() const;
    const std::vector<std::unique_ptr<Zone>>& zones() const;

private:
    std::vector<std::vector<std::unique_ptr<Zone>>> pages;
    std::vector<std::string> pageNames;
    int currentPage = 0;
};
}
