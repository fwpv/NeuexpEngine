#pragma once

#include "element.h"
#include "input.h"

#include <optional>

namespace NeuexpEngine::Gui
{
// Порядок детей определяет рисование; ввод обходит тот же список с конца.
// Контекст владеет активными страницами. Обработчики могут менять дерево и страницы:
// текущая доставка удерживает элемент, перед следующей целью дерево перечитывается.
class Context
{
public:
    ~Context();
    // Масштаб необязателен (1 по умолчанию), не зависит от последующих изменений DPI.
    bool setViewport(const Viewport& viewport, float scale = 1);
    void setScreen(std::shared_ptr<Screen> screen);
    void setModal(std::shared_ptr<Screen> screen);
    std::shared_ptr<Screen> screen() const;
    std::shared_ptr<Screen> modal() const;
    // true: ввод поглощён GUI, передавать его игровому миру не следует.
    bool handle(const NeuexpEngine::Event& event);
    // Вызывать после setWorldView; устанавливает собственный пиксельный UiView.
    bool draw(Renderer2D& renderer, ResourceError& error);
    std::optional<Rect> bounds(const Element& element);

private:
    struct Entry
    {
        std::shared_ptr<Element> element;
        Rect bounds;
        Rect clip;
        float scale = 1;
        bool enabled = true;
        bool modal = false;
    };
    void rebuild();
    void append(const std::shared_ptr<Element>& element, const Rect& bounds,
        const Rect& clip, float scale, bool enabled, bool modal);
    void cancel();
    void reconcile();
    void updateHover();
    std::shared_ptr<Element> hit() const;
    bool available(const std::shared_ptr<Element>& element) const;

    std::shared_ptr<Screen> page;
    std::shared_ptr<Screen> overlay;
    std::shared_ptr<Element> hover;
    std::shared_ptr<Element> capture;
    std::vector<Entry> entries;
    UiView view;
    float scale = 1;
    Vector2 cursor;
    bool hasCursor = false;
    bool viewportValid = false;
};
}
