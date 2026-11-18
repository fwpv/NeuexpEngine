#include "context.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace NeuexpEngine::Gui
{
namespace
{
bool containsPointer(const Rect& area, Vector2 point)
{
    return point.x >= area.position.x && point.y >= area.position.y
        && point.x < area.position.x + area.size.width && point.y < area.position.y + area.size.height;
}
Rect intersect(const Rect& a, const Rect& b)
{
    const float x = std::max(a.position.x, b.position.x);
    const float y = std::max(a.position.y, b.position.y);
    return {x, y, std::max(0.0F, std::min(a.position.x + a.size.width, b.position.x + b.size.width) - x),
        std::max(0.0F, std::min(a.position.y + a.size.height, b.position.y + b.size.height) - y)};
}
float aligned(float available, float size, Align alignment)
{
    return alignment == Align::Center ? (available - size) / 2 : alignment == Align::End ? available - size : 0;
}
}

Context::~Context()
{
    // При уничтожении контекста пользовательские обработчики уже могут быть недоступны.
    if (capture) { capture->down = false; }
    if (hover) { hover->over = false; }
}
bool Context::setViewport(const Viewport& area, float newScale)
{
    UiError error;
    if (!std::isfinite(newScale) || newScale <= 0 || !UiCanvas{}.createView(area, view, error)) {
        viewportValid = false;
        cancel();
        return false;
    }
    scale = newScale;
    viewportValid = true;
    rebuild();
    reconcile();
    return true;
}
void Context::setScreen(std::shared_ptr<Screen> value)
{
    page = std::move(value);
    overlay.reset();
    cancel();
    rebuild();
}
void Context::setModal(std::shared_ptr<Screen> value)
{
    overlay = std::move(value);
    cancel();
    rebuild();
}
std::shared_ptr<Screen> Context::screen() const { return page; }
std::shared_ptr<Screen> Context::modal() const { return overlay; }
void Context::cancel()
{
    auto oldCapture = std::move(capture);
    auto oldHover = std::move(hover);
    if (oldCapture) { oldCapture->down = false; }
    if (oldHover) { oldHover->over = false; }
    if (oldCapture) { oldCapture->emit(EventType::Cancel); }
    if (oldHover) { oldHover->emit(EventType::Leave); }
}

void Context::append(const std::shared_ptr<Element>& element, const Rect& bounds,
    const Rect& parentClip, float elementScale, bool enabled, bool isModal)
{
    if (!element || !element->visible()) {
        return;
    }
    enabled = enabled && element->enabled();
    const Rect clip = intersect(parentClip, bounds);
    entries.push_back(Entry{element, bounds, clip, elementScale, enabled, isModal});
    const auto container = std::dynamic_pointer_cast<Container>(element);
    if (!container) {
        return;
    }
    const auto& layout = container->layout();
    const float padding = layout.padding * elementScale;
    const Rect content{bounds.position.x + padding, bounds.position.y + padding,
        std::max(0.0F, bounds.size.width - 2 * padding), std::max(0.0F, bounds.size.height - 2 * padding)};
    float total = 0;
    std::size_t count = 0;
    for (const auto& child : container->children()) {
        if (!child->visible()) { continue; }
        const auto& placement = child->placement();
        const float childScale = placement.scaleMode == ScaleMode::Pixels ? 1 : elementScale;
        total += (layout.flow == Flow::Row ? placement.size.width : placement.size.height) * childScale;
        ++count;
    }
    if (count > 1) { total += static_cast<float>(count - 1) * layout.spacing * elementScale; }
    float position = aligned(layout.flow == Flow::Row ? content.size.width : content.size.height,
        total, layout.alignment);
    if (container->scrollable() && layout.flow == Flow::Column) {
        const float maximum = std::max(0.0F, total - content.size.height);
        container->setScrollOffset(std::min(container->scrollOffset(), maximum / elementScale));
        position = std::max(0.0F, position) - container->scrollOffset() * elementScale;
    }
    for (const auto& child : container->children()) {
        if (!child->visible()) { continue; }
        const auto& placement = child->placement();
        const float childScale = placement.scaleMode == ScaleMode::Pixels ? 1 : elementScale;
        Size2 size{placement.size.width * childScale, placement.size.height * childScale};
        if (placement.horizontal == Align::Stretch && layout.flow != Flow::Row) { size.width = content.size.width; }
        if (placement.vertical == Align::Stretch && layout.flow != Flow::Column) { size.height = content.size.height; }
        Vector2 offset{aligned(content.size.width, size.width, placement.horizontal),
            aligned(content.size.height, size.height, placement.vertical)};
        if (layout.flow == Flow::Row) {
            offset.x = position;
            position += size.width + layout.spacing * elementScale;
        } else if (layout.flow == Flow::Column) {
            offset.y = position;
            position += size.height + layout.spacing * elementScale;
        }
        offset += placement.offset * childScale;
        append(child, Rect{content.position + offset, size}, intersect(clip, content), childScale, enabled, isModal);
    }
}
void Context::rebuild()
{
    entries.clear();
    if (!viewportValid) { return; }
    const Rect area{0, 0, view.size.width, view.size.height};
    append(page, area, area, scale, true, false);
    append(overlay, area, area, scale, true, true);
}
bool Context::available(const std::shared_ptr<Element>& element) const
{
    return element && std::any_of(entries.begin(), entries.end(), [&](const Entry& entry) {
        return entry.element == element && entry.enabled && (!overlay || entry.modal)
            && entry.clip.size.width > 0 && entry.clip.size.height > 0;
    });
}
void Context::reconcile()
{
    if ((capture && !available(capture)) || (hover && !available(hover))) {
        cancel();
        rebuild();
    }
}
std::shared_ptr<Element> Context::hit() const
{
    if (!hasCursor) { return {}; }
    for (auto entry = entries.rbegin(); entry != entries.rend(); ++entry) {
        if ((!overlay || entry->modal) && containsPointer(entry->clip, cursor)
            && entry->element->pointerPolicy() == PointerPolicy::Block) {
            return entry->element;
        }
    }
    return {};
}
void Context::updateHover()
{
    auto target = hit();
    if (!available(target) || !target->interactive()) { target.reset(); }
    if (hover == target) { return; }
    auto previous = std::move(hover);
    if (previous) {
        previous->over = false;
        previous->emit(EventType::Leave);
    }
    rebuild();
    target = hit();
    if (available(target) && target->interactive()) {
        hover = target;
        hover->over = true;
        target->emit(EventType::Enter);
    }
    rebuild();
    reconcile();
}
bool Context::handle(const NeuexpEngine::Event& event)
{
    rebuild();
    reconcile();
    if (event.type == NeuexpEngine::EventType::FocusLost || event.type == NeuexpEngine::EventType::MouseCaptureLost
        || event.type == NeuexpEngine::EventType::WindowMinimized || event.type == NeuexpEngine::EventType::WindowDestroyed) {
        const bool consumed = static_cast<bool>(capture);
        hasCursor = false;
        cancel();
        return consumed;
    }
    if (event.type == NeuexpEngine::EventType::MouseLeft) {
        hasCursor = false;
        updateHover();
        return static_cast<bool>(capture) || static_cast<bool>(overlay);
    }
    const bool mouse = event.type == NeuexpEngine::EventType::MouseMoved || event.type == NeuexpEngine::EventType::MousePressed
        || event.type == NeuexpEngine::EventType::MouseReleased || event.type == NeuexpEngine::EventType::MouseWheel
        || event.type == NeuexpEngine::EventType::MouseEntered;
    if (!mouse) { return false; }
    cursor = view.screenToUiTransform.transformPoint(Vector2{static_cast<float>(event.x), static_cast<float>(event.y)});
    hasCursor = viewportValid;
    updateHover();
    auto target = hit();
    const bool consumed = static_cast<bool>(target) || static_cast<bool>(capture) || static_cast<bool>(overlay);
    if (event.canceled) {
        cancel();
        return consumed;
    }
    if (event.type == NeuexpEngine::EventType::MouseWheel) {
        for (Element* ancestor = target.get(); ancestor; ancestor = ancestor->parent) {
            const auto container = dynamic_cast<Container*>(ancestor);
            if (container && container->scrollable() && container->layout().flow == Flow::Column) {
                container->setScrollOffset(container->scrollOffset() - static_cast<float>(event.wheelY) * 32);
                rebuild();
                updateHover();
                break;
            }
        }
        return consumed;
    }
    if (event.button != MouseButton::Left) { return consumed; }
    if (event.type == NeuexpEngine::EventType::MousePressed) {
        if (!capture && available(target) && target->interactive()) {
            capture = target;
            target->down = true;
            target->emit(EventType::Press);
            rebuild();
            reconcile();
        }
    } else if (event.type == NeuexpEngine::EventType::MouseReleased && capture) {
        auto released = std::move(capture);
        released->down = false;
        const bool clicked = target == released;
        released->emit(EventType::Release);
        rebuild();
        if (clicked && available(released) && hit() == released) {
            released->activate();
            rebuild();
            if (available(released)) { released->emit(EventType::Click); }
        }
        rebuild();
        reconcile();
    }
    return consumed;
}
std::optional<Rect> Context::bounds(const Element& element)
{
    rebuild();
    for (const auto& entry : entries) {
        if (entry.element.get() == &element) { return entry.bounds; }
    }
    return std::nullopt;
}
bool Context::draw(Renderer2D& renderer, ResourceError& error)
{
    error = {};
    rebuild();
    reconcile();
    updateHover();
    if (!viewportValid) {
        error = ResourceError{ResourceErrorCode::InvalidState, "GUI viewport is not configured."};
        return false;
    }
    renderer.setUiView(view);
    for (const auto& entry : entries) {
        const auto& clip = entry.clip;
        const int left = static_cast<int>(std::ceil(clip.position.x));
        const int top = static_cast<int>(std::ceil(clip.position.y));
        const int right = static_cast<int>(std::ceil(clip.position.x + clip.size.width));
        const int bottom = static_cast<int>(std::ceil(clip.position.y + clip.size.height));
        renderer.setClip(Viewport{view.viewport.x + left, view.viewport.y + top, right - left, bottom - top});
        const State state = !entry.enabled ? State::Disabled : entry.element->pressed() ? State::Pressed
            : entry.element->hovered() ? State::Hover : State::Normal;
        if (!entry.element->paint(renderer, entry.bounds, entry.scale, state, error)) {
            renderer.setClip(std::nullopt);
            return false;
        }
    }
    renderer.setClip(std::nullopt);
    return true;
}
}
