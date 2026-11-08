#include "element.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <utility>

namespace Engine::Gui
{
void Element::setPlacement(const Placement& value)
{
    location = value;
}
const Placement& Element::placement() const
{
    return location;
}
void Element::setVisible(bool value)
{
    shown = value;
}
bool Element::visible() const
{
    return shown;
}
void Element::setEnabled(bool value)
{
    active = value;
}
bool Element::enabled() const
{
    return active;
}
void Element::setPointerPolicy(PointerPolicy value)
{
    pointer = value;
}
PointerPolicy Element::pointerPolicy() const
{
    return pointer;
}
Style& Element::style()
{
    return appearance;
}
const Style& Element::style() const
{
    return appearance;
}
bool Element::hovered() const
{
    return over;
}
bool Element::pressed() const
{
    return down;
}
bool Element::interactive() const
{
    return false;
}
void Element::activate()
{
}
Subscription Element::subscribe(EventType type, Events::Handler handler)
{
    return events.subscribe(type, std::move(handler));
}
void Element::emit(EventType type, bool checked)
{
    events.emit(Event{type, checked});
}

bool Element::drawText(Renderer2D& renderer, const std::string& text, const Rect& bounds,
    float scale, State state, ResourceError& error) const
{
    if (text.empty()) {
        return true;
    }
    const auto& appearance = style();
    const float padding = appearance.padding * scale;
    const float width = std::max(0.0F, bounds.size.width - 2 * padding);
    DrawOptions options;
    options.color = appearance.foreground;
    if (state == State::Disabled) {
        options.color.alpha *= 0.45F;
    }
    if (appearance.font.isValid()) {
        TextLayout textLayout;
        TextLayoutOptions layout;
        layout.width = width;
        layout.scale = scale * appearance.textScale;
        layout.alignment = appearance.textAlignment;
        if (!textLayout.build(appearance.font, text, error, layout)) {
            return false;
        }
        renderer.drawText(textLayout, Vector2{bounds.position.x + padding,
            bounds.position.y + (bounds.size.height - textLayout.size().height) / 2}, options);
    } else {
        const float pixel = scale * appearance.textScale * appearance.debugPixel;
        const auto size = Renderer2D::measureDebugText(text, pixel);
        const float offset = appearance.textAlignment == TextAlignment::Center ? (width - size.width) / 2
            : appearance.textAlignment == TextAlignment::Right ? width - size.width : 0;
        renderer.drawDebugText(text, Vector2{bounds.position.x + padding + offset,
            bounds.position.y + (bounds.size.height - size.height) / 2}, options, pixel);
    }
    return true;
}

void Element::drawSurface(Renderer2D& renderer, const Rect& bounds, State state, bool checked) const
{
    const auto& appearance = style();
    const auto& images = checked && appearance.checkedImages[0].texture.isValid()
        ? appearance.checkedImages : appearance.images;
    Sprite image = images[static_cast<std::size_t>(state)];
    const bool fallback = !image.texture.isValid();
    if (fallback) {
        image = images[0];
    }
    if (image.texture.isValid()) {
        image.size = bounds.size;
        image.transform = Transform2{bounds.position, 0, Vector2{1, 1}, Vector2{}};
        if (fallback && state == State::Disabled) {
            image.color.alpha *= 0.4F;
        } else if (fallback && state == State::Pressed) {
            image.color = Color{0.65F, 0.65F, 0.65F, 1};
        } else if (fallback && state == State::Hover) {
            image.color = Color{0.85F, 0.95F, 1, 1};
        }
        renderer.drawSprite(image);
        return;
    }
    Color color = state == State::Pressed ? appearance.pressed
        : state == State::Hover ? appearance.hover : appearance.background;
    if (state == State::Disabled) {
        color.alpha *= 0.45F;
    }
    // GUI рисуется в экранных пикселях. Округление совпадает с отсечением Context:
    // линия по границе могла попасть за scissor и потерять одну из сторон.
    const float left = std::ceil(bounds.position.x);
    const float top = std::ceil(bounds.position.y);
    const float right = std::ceil(bounds.position.x + bounds.size.width);
    const float bottom = std::ceil(bounds.position.y + bounds.size.height);
    const float width = right - left;
    const float height = bottom - top;
    if (width <= 0 || height <= 0) {
        return;
    }
    renderer.drawRectangle(Rect{left, top, width, height}, color);
    // Полосы не перекрываются: прозрачная рамка смешивается один раз и в углах.
    renderer.drawRectangle(Rect{left, top, width, 1}, appearance.border);
    if (height > 1) {
        renderer.drawRectangle(Rect{left, bottom - 1, width, 1}, appearance.border);
    }
    if (height > 2) {
        renderer.drawRectangle(Rect{left, top + 1, 1, height - 2}, appearance.border);
        if (width > 1) {
            renderer.drawRectangle(Rect{right - 1, top + 1, 1, height - 2}, appearance.border);
        }
    }
}

Container::~Container()
{
    clear();
}
bool Container::add(const std::shared_ptr<Element>& element)
{
    if (!element || element->parent) {
        return false;
    }
    for (const Element* ancestor = this; ancestor; ancestor = ancestor->parent) {
        if (ancestor == element.get()) {
            return false;
        }
    }
    element->parent = this;
    items.push_back(element);
    return true;
}
bool Container::remove(const std::shared_ptr<Element>& element)
{
    const auto found = std::find(items.begin(), items.end(), element);
    if (found == items.end()) {
        return false;
    }
    (*found)->parent = nullptr;
    items.erase(found);
    return true;
}
void Container::clear()
{
    for (const auto& element : items) {
        element->parent = nullptr;
    }
    items.clear();
}
const std::vector<std::shared_ptr<Element>>& Container::children() const
{
    return items;
}
Layout& Container::layout()
{
    return arrangement;
}
const Layout& Container::layout() const
{
    return arrangement;
}
void Container::setBackgroundVisible(bool value)
{
    backgroundVisible = value;
}
void Container::setScrollable(bool value)
{
    verticalScroll = value;
}
bool Container::scrollable() const
{
    return verticalScroll;
}
void Container::setScrollOffset(float value)
{
    scroll = std::max(0.0F, value);
}
float Container::scrollOffset() const
{
    return scroll;
}
bool Container::paint(Renderer2D& renderer, const Rect& bounds, float, State state, ResourceError&) const
{
    if (backgroundVisible) {
        drawSurface(renderer, bounds, state);
    }
    return true;
}

Label::Label(std::string text)
    : caption(std::move(text))
{
}
void Label::setText(std::string text)
{
    caption = std::move(text);
}
const std::string& Label::text() const
{
    return caption;
}
bool Label::paint(Renderer2D& renderer, const Rect& bounds, float scale, State state, ResourceError& error) const
{
    if (style().images[0].texture.isValid()) {
        drawSurface(renderer, bounds, state);
    }
    return drawText(renderer, caption, bounds, scale, state, error);
}
Button::Button(std::string text)
    : caption(std::move(text))
{
    setPointerPolicy(PointerPolicy::Block);
}
void Button::setText(std::string text)
{
    caption = std::move(text);
}
const std::string& Button::text() const
{
    return caption;
}
bool Button::interactive() const
{
    return true;
}
bool Button::paint(Renderer2D& renderer, const Rect& bounds, float scale, State state, ResourceError& error) const
{
    drawSurface(renderer, bounds, state);
    return drawText(renderer, caption, bounds, scale, state, error);
}
Checkbox::Checkbox(std::string text)
    : caption(std::move(text))
{
    setPointerPolicy(PointerPolicy::Block);
}
void Checkbox::setChecked(bool value)
{
    selected = value;
}
bool Checkbox::checked() const
{
    return selected;
}
bool Checkbox::interactive() const
{
    return true;
}
void Checkbox::activate()
{
    selected = !selected;
    emit(EventType::Change, selected);
}
bool Checkbox::paint(Renderer2D& renderer, const Rect& bounds, float scale, State state, ResourceError& error) const
{
    const float side = std::min(bounds.size.height, 24 * scale);
    const Rect box{bounds.position.x, bounds.position.y + (bounds.size.height - side) / 2, side, side};
    drawSurface(renderer, box, state, selected);
    if (selected && !style().checkedImages[0].texture.isValid()) {
        const Color color = style().accent;
        renderer.drawLine(Vector2{box.position.x + side * 0.2F, box.position.y + side * 0.5F},
            Vector2{box.position.x + side * 0.43F, box.position.y + side * 0.75F}, color);
        renderer.drawLine(Vector2{box.position.x + side * 0.43F, box.position.y + side * 0.75F},
            Vector2{box.position.x + side * 0.85F, box.position.y + side * 0.2F}, color);
    }
    return drawText(renderer, caption, Rect{bounds.position.x + side, bounds.position.y,
        std::max(0.0F, bounds.size.width - side), bounds.size.height}, scale, state, error);
}
void ProgressBar::setValue(float value)
{
    progress = std::clamp(value, 0.0F, 1.0F);
}
float ProgressBar::value() const
{
    return progress;
}
bool ProgressBar::paint(Renderer2D& renderer, const Rect& bounds, float, State state, ResourceError&) const
{
    drawSurface(renderer, bounds, state);
    const float inset = std::min(2.0F, std::min(bounds.size.width, bounds.size.height) / 2);
    renderer.drawRectangle(Rect{bounds.position.x + inset, bounds.position.y + inset,
        (bounds.size.width - 2 * inset) * progress, bounds.size.height - 2 * inset}, style().accent);
    return true;
}
void NumericIndicator::setValue(double value, int precision)
{
    number = value;
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(std::clamp(precision, 0, 9)) << value;
    caption = stream.str();
}
double NumericIndicator::value() const
{
    return number;
}
bool NumericIndicator::paint(Renderer2D& renderer, const Rect& bounds, float scale, State state, ResourceError& error) const
{
    if (style().images[0].texture.isValid()) {
        drawSurface(renderer, bounds, state);
    }
    return drawText(renderer, caption, bounds, scale, state, error);
}
}
