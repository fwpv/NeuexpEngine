#pragma once

#include "events.h"
#include "renderer.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace Engine::Gui
{
enum class Align { Start, Center, End, Stretch };
enum class ScaleMode { Inherit, Pixels };
enum class Flow { Manual, Row, Column };
enum class PointerPolicy { PassThrough, Block };
enum class State { Normal, Hover, Pressed, Disabled };

// Размеры и отступы неотрицательны, все числа конечны. Pixels отменяет масштаб
// для размеров, текста и потомков, но не отменяет выравнивание в родителе.
struct Placement
{
    Size2 size = {180, 40};
    Vector2 offset;
    Align horizontal = Align::Start;
    Align vertical = Align::Start;
    ScaleMode scale = ScaleMode::Inherit;
};

struct Layout
{
    Flow flow = Flow::Manual;
    float padding = 0;
    float spacing = 8;
    Align alignment = Align::Start;
};

struct Style
{
    Color background = {0.12F, 0.18F, 0.25F, 1};
    Color hover = {0.2F, 0.34F, 0.45F, 1};
    Color pressed = {0.08F, 0.13F, 0.2F, 1};
    Color border = {0.4F, 0.62F, 0.73F, 1};
    Color foreground = {0.93F, 0.96F, 1, 1};
    Color accent = {0.2F, 0.75F, 0.55F, 1};
    BitmapFont font;
    TextureFilter textFilter = TextureFilter::Linear;
    float textScale = 1;
    float debugPixel = 2;
    float padding = 8;
    TextAlignment textAlignment = TextAlignment::Center;
    // Normal, Hover, Pressed, Disabled. Пустое состояние использует Normal с оттенком.
    std::array<Sprite, 4> images;
    std::array<Sprite, 4> checkedImages;
};

class Container;
class Context;

class Element
{
public:
    virtual ~Element() = default;
    Element(const Element&) = delete;
    Element& operator=(const Element&) = delete;
    void setPlacement(const Placement& placement);
    const Placement& placement() const;
    void setVisible(bool visible);
    bool visible() const;
    void setEnabled(bool enabled);
    bool enabled() const;
    void setPointerPolicy(PointerPolicy policy);
    PointerPolicy pointerPolicy() const;
    Style& style();
    const Style& style() const;
    [[nodiscard]] Subscription subscribe(EventType type, Events::Handler handler);
    bool hovered() const;
    bool pressed() const;

private:
    friend class Container;
    friend class Context;
    virtual bool interactive() const;
    virtual void activate();
    virtual bool paint(Renderer2D& renderer, const Rect& bounds, float scale,
        State state, ResourceError& error) const = 0;
    Container* parent = nullptr;
    Placement location;
    Style appearance;
    Events events;
    PointerPolicy pointer = PointerPolicy::PassThrough;
    bool shown = true;
    bool active = true;
    bool over = false;
    bool down = false;

protected:
    Element() = default;
    void emit(EventType type, bool checked = false);
    bool drawText(Renderer2D& renderer, const std::string& text, const Rect& bounds,
        float scale, State state, ResourceError& error) const;
    void drawSurface(Renderer2D& renderer, const Rect& bounds, State state, bool checked = false) const;
};

class Container : public Element
{
public:
    ~Container() override;
    // Один родитель; циклы и повторное добавление отклоняются.
    bool add(const std::shared_ptr<Element>& element);
    bool remove(const std::shared_ptr<Element>& element);
    void clear();
    const std::vector<std::shared_ptr<Element>>& children() const;
    Layout& layout();
    const Layout& layout() const;
    void setBackgroundVisible(bool visible);
    // Вертикальная прокрутка для Column, в локальных единицах контейнера.
    void setScrollable(bool scrollable);
    bool scrollable() const;
    void setScrollOffset(float offset);
    float scrollOffset() const;

private:
    bool paint(Renderer2D& renderer, const Rect& bounds, float scale,
        State state, ResourceError& error) const override;
    std::vector<std::shared_ptr<Element>> items;
    Layout arrangement;
    bool backgroundVisible = false;
    bool verticalScroll = false;
    float scroll = 0;
};

class Screen : public Container
{
};

class Label : public Element
{
public:
    explicit Label(std::string text = {});
    void setText(std::string text);
    const std::string& text() const;

private:
    bool paint(Renderer2D& renderer, const Rect& bounds, float scale,
        State state, ResourceError& error) const override;
    std::string caption;
};

class Button : public Element
{
public:
    explicit Button(std::string text = {});
    void setText(std::string text);
    const std::string& text() const;

private:
    bool interactive() const override;
    bool paint(Renderer2D& renderer, const Rect& bounds, float scale,
        State state, ResourceError& error) const override;
    std::string caption;
};

class Checkbox : public Element
{
public:
    explicit Checkbox(std::string text = {});
    void setChecked(bool checked);
    bool checked() const;

private:
    bool interactive() const override;
    void activate() override;
    bool paint(Renderer2D& renderer, const Rect& bounds, float scale,
        State state, ResourceError& error) const override;
    std::string caption;
    bool selected = false;
};

class ProgressBar : public Element
{
public:
    void setValue(float value);
    float value() const;

private:
    bool paint(Renderer2D& renderer, const Rect& bounds, float scale,
        State state, ResourceError& error) const override;
    float progress = 0;
};

class NumericIndicator : public Element
{
public:
    void setValue(double value, int precision = 0);
    double value() const;

private:
    bool paint(Renderer2D& renderer, const Rect& bounds, float scale,
        State state, ResourceError& error) const override;
    double number = 0;
    std::string caption = "0";
};
}
