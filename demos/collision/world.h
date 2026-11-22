#pragma once

#include "collision/collider.h"
#include "collision/contacts.h"
#include "core/geometry.h"
#include "core/math2d.h"
#include "graphics/renderer.h"
#include "scene/object.h"
#include "scene/scene.h"

#include <memory>
#include <string>
#include <vector>

namespace CollisionDemo
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

// Слои проверки масок: формы носят первый слой, плита в зоне масок — второй.
constexpr NeuexpEngine::CollisionLayer ShapeLayer = 1U;
constexpr NeuexpEngine::CollisionLayer SlabLayer = 2U;

// Оформление формы: полупрозрачная заливка и контур по виду формы.
struct ShapeStyle
{
    NeuexpEngine::Color fill;
    NeuexpEngine::Color outline;
};

// Зона проверки: собственный прямоугольник, своя сцена с формами и подпись рядом
// о том, что обнаружено в этот момент. Зоны не влияют друг на друга.
class Zone
{
public:
    Zone(NeuexpEngine::Rect area, std::string title);
    virtual ~Zone() = default;
    Zone(const Zone&) = delete;
    Zone& operator=(const Zone&) = delete;

    const NeuexpEngine::Rect& area() const { return zoneArea; }
    const std::string& title() const { return zoneTitle; }

    // Накапливает время, двигает объекты и выполняет проверку своего вида.
    virtual void update(float deltaTime);
    // Что обнаружено сейчас.
    virtual std::string state() const = 0;
    // Счётчики переходов контакта; пусто, если зона их не ведёт.
    virtual std::string counters() const { return {}; }
    // Признак активного контакта: рамка зоны подсвечивается.
    bool active() const { return contactActive; }
    // Переключатели зоны: триггер и маска. Зона без такого варианта не реагирует.
    virtual bool toggleTrigger() { return false; }
    virtual bool toggleMask() { return false; }

    // Рамка зоны, заголовок, формы, дополнительные отметки и подписи.
    void draw(NeuexpEngine::Renderer2D& drawing) const;

protected:
    NeuexpEngine::Scene& scene() { return ownedScene; }
    const NeuexpEngine::Scene& scene() const { return ownedScene; }
    float elapsedSeconds() const { return elapsed; }
    void markContactActive(bool value) { contactActive = value; }

    // Движение объектов зоны; вызывается после накопления времени.
    virtual void move() = 0;
    // Отметки поверх форм: маркеры контактов, мера расстояния, луч, точка.
    virtual void drawExtra(NeuexpEngine::Renderer2D& drawing) const = 0;
    virtual ShapeStyle styleOf(const NeuexpEngine::GameObject& object) const;

private:
    NeuexpEngine::Rect zoneArea;
    std::string zoneTitle;
    NeuexpEngine::Scene ownedScene;
    float elapsed = 0;
    bool contactActive = false;
};

// Зона контактов: своя сцена, свой трекер и счётчики переходов.
class ContactZone : public Zone
{
public:
    ContactZone(NeuexpEngine::Rect area, std::string title);
    void update(float deltaTime) override;
    std::string counters() const override;

protected:
    const std::vector<NeuexpEngine::Contact>& contacts() const { return currentContacts; }
    void drawExtra(NeuexpEngine::Renderer2D& drawing) const override;

private:
    NeuexpEngine::ContactTracker tracker;
    std::vector<NeuexpEngine::Contact> currentContacts;
    int begun = 0;
    int ended = 0;
};

// Фабрика проверок: страницы зон, их обновление, рисование и переключатели.
class CheckBoard
{
public:
    CheckBoard();

    // Возвращает все зоны в исходное состояние.
    void restart();
    void update(float deltaTime);
    void draw(NeuexpEngine::Renderer2D& drawing) const;

    void nextPage();
    int pageIndex() const { return currentPage; }
    int pageCount() const;
    const std::string& pageName() const;

    // Переключает триггер и маску в зонах текущей страницы.
    void toggleTrigger();
    void toggleMask();

    // Зоны текущей страницы: заголовки, состояния и счётчики для инструментов проверки.
    const std::vector<std::unique_ptr<Zone>>& zones() const;

private:
    void build();

    std::vector<std::vector<std::unique_ptr<Zone>>> pages;
    std::vector<std::string> pageNames;
    int currentPage = 0;
};
}
