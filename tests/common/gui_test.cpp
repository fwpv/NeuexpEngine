#include "gui/context.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using namespace NeuexpEngine;
namespace UI = NeuexpEngine::Gui;
void require(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}
bool near(float a, float b)
{
    return std::abs(a - b) < 0.001F;
}
NeuexpEngine::Event mouse(NeuexpEngine::EventType type, int x, int y)
{
    NeuexpEngine::Event event;
    event.type = type;
    event.button = MouseButton::Left;
    event.x = x;
    event.y = y;
    return event;
}
void click(UI::Context& context, int x, int y)
{
    context.handle(mouse(NeuexpEngine::EventType::MousePressed, x, y));
    context.handle(mouse(NeuexpEngine::EventType::MouseReleased, x, y));
}
void routing()
{
    UI::Context context;
    require(context.setViewport(Viewport{20, 30, 400, 300}), "Viewport failed");
    auto screen = std::make_shared<UI::Screen>();
    auto lower = std::make_shared<UI::Button>("Lower");
    auto upper = std::make_shared<UI::Button>("Upper");
    screen->add(lower);
    screen->add(upper);
    context.setScreen(screen);
    int lowerClicks = 0;
    int upperClicks = 0;
    auto first = lower->subscribe(UI::EventType::Click, [&](const UI::Event&) { ++lowerClicks; });
    auto second = upper->subscribe(UI::EventType::Click, [&](const UI::Event&) { ++upperClicks; });
    click(context, 30, 40);
    require(lowerClicks == 0 && upperClicks == 1, "Draw order did not select upper button");
    second.disconnect();
    click(context, 30, 40);
    require(lowerClicks == 0, "Button without subscriber passed input through");
    upper->setEnabled(false);
    click(context, 30, 40);
    require(lowerClicks == 0, "Disabled button passed input through");
    upper->setVisible(false);
    click(context, 30, 40);
    require(lowerClicks == 1, "Hidden button blocked input");
    auto decoration = std::make_shared<UI::Label>("Decoration");
    screen->add(decoration);
    click(context, 30, 40);
    require(lowerClicks == 2, "Label blocked input");
    decoration->setPointerPolicy(UI::PointerPolicy::Block);
    click(context, 30, 40);
    require(lowerClicks == 2, "Blocking label passed input");
    decoration->setVisible(false);
    context.handle(mouse(NeuexpEngine::EventType::MousePressed, 30, 40));
    require(lower->pressed(), "Button did not capture press");
    context.handle(mouse(NeuexpEngine::EventType::MouseReleased, 300, 200));
    require(!lower->pressed() && lowerClicks == 2, "Outside release clicked");
    context.handle(mouse(NeuexpEngine::EventType::MousePressed, 300, 200));
    context.handle(mouse(NeuexpEngine::EventType::MouseReleased, 30, 40));
    require(lowerClicks == 2, "Outside press clicked");
    context.handle(mouse(NeuexpEngine::EventType::MousePressed, 30, 40));
    auto event = mouse(NeuexpEngine::EventType::MouseReleased, 30, 40);
    event.canceled = true;
    context.handle(event);
    require(!lower->pressed() && lowerClicks == 2, "Canceled release clicked");
    context.handle(mouse(NeuexpEngine::EventType::MousePressed, 30, 40));
    event.type = NeuexpEngine::EventType::FocusLost;
    context.handle(event);
    context.handle(mouse(NeuexpEngine::EventType::MouseReleased, 30, 40));
    require(lowerClicks == 2 && !lower->pressed(), "Focus loss left capture");
    click(context, 200, 40);
    require(lowerClicks == 2, "Right boundary included");
    require(!context.handle(mouse(NeuexpEngine::EventType::MousePressed, 500, 500)), "Empty area consumed input");
}
void mutation()
{
    UI::Context context;
    context.setViewport(Viewport{0, 0, 400, 300});
    auto screen = std::make_shared<UI::Screen>();
    auto next = std::make_shared<UI::Screen>();
    auto button = std::make_shared<UI::Button>();
    auto other = std::make_shared<UI::Button>();
    screen->add(button);
    next->add(other);
    context.setScreen(screen);
    int clicks = 0;
    auto clicked = button->subscribe(UI::EventType::Click, [&](const UI::Event&) { ++clicks; });
    auto press = button->subscribe(UI::EventType::Press, [&](const UI::Event&) { context.setScreen(next); });
    click(context, 10, 10);
    require(clicks == 0 && !button->pressed(), "Page switch retained captured element");
    press.disconnect();
    context.setScreen(screen);
    auto release = button->subscribe(UI::EventType::Release, [&](const UI::Event&) { screen->remove(button); });
    click(context, 10, 10);
    require(clicks == 0 && !button->pressed(), "Removed element activated on release");
    release.disconnect();
    screen->add(button);
    auto removeOnEnter = button->subscribe(UI::EventType::Enter, [&](const UI::Event&) { screen->remove(button); });
    context.handle(mouse(NeuexpEngine::EventType::MouseMoved, 10, 10));
    require(!button->hovered(), "Removed hover retained state");
    removeOnEnter.disconnect();
    screen->add(button);
    auto check = std::make_shared<UI::Checkbox>();
    screen->add(check);
    int changed = 0;
    auto change = check->subscribe(UI::EventType::Change, [&](const UI::Event& event) {
        require(event.checked, "Checkbox event value incorrect");
        ++changed;
        context.setScreen(next);
    });
    click(context, 10, 10);
    require(changed == 1 && check->checked(), "Checkbox change failed");
    context.setScreen(screen);
    auto modal = std::make_shared<UI::Screen>();
    context.setModal(modal);
    require(context.handle(mouse(NeuexpEngine::EventType::MousePressed, 350, 250)), "Empty modal passed input to world");
    click(context, 10, 10);
    require(changed == 1 && clicks == 0, "Modal passed input to lower page");
    context.setModal({});
    check->setVisible(false);
    click(context, 10, 10);
    require(clicks == 1, "Modal close did not restore page");
    context.handle(mouse(NeuexpEngine::EventType::MousePressed, 10, 10));
    screen->setEnabled(false);
    context.handle(mouse(NeuexpEngine::EventType::MouseReleased, 10, 10));
    require(clicks == 1 && !button->pressed(), "Disabled ancestor retained press");
}
void subscriptions()
{
    auto button = std::make_shared<UI::Button>();
    auto screen = std::make_shared<UI::Screen>();
    screen->add(button);
    UI::Context context;
    context.setViewport(Viewport{0, 0, 400, 300});
    context.setScreen(screen);
    std::vector<int> order;
    UI::Subscription second;
    UI::Subscription added;
    auto first = button->subscribe(UI::EventType::Click, [&](const UI::Event&) {
        order.push_back(1);
        second.disconnect();
        if (!added.connected()) {
            added = button->subscribe(UI::EventType::Click, [&](const UI::Event&) { order.push_back(3); });
        }
    });
    second = button->subscribe(UI::EventType::Click, [&](const UI::Event&) { order.push_back(2); });
    click(context, 10, 10);
    require(order == std::vector<int>{1}, "Subscription snapshot incorrect");
    click(context, 10, 10);
    require(order == std::vector<int>({1, 1, 3}), "Subscriber order incorrect");
    auto moved = std::move(first);
    require(moved.connected() && !first.connected(), "Moved subscription invalid");
    context.setScreen({});
    screen->clear();
    button.reset();
    require(!moved.connected() && !added.connected(), "Subscription survived emitter destruction as connected");
}
void actionSubscription()
{
    auto button = std::make_shared<UI::Button>();
    auto screen = std::make_shared<UI::Screen>();
    screen->add(button);
    UI::Context context;
    context.setViewport(Viewport{0, 0, 400, 300});
    context.setScreen(screen);
    int clicks = 0;
    auto subscription = button->subscribe(UI::EventType::Click, [&] { ++clicks; });
    click(context, 10, 10);
    require(clicks == 1, "Action subscriber did not fire");
}
void layout()
{
    UI::Context context;
    context.setViewport(Viewport{0, 0, 800, 600}, 2);
    auto screen = std::make_shared<UI::Screen>();
    auto panel = std::make_shared<UI::Container>();
    UI::Placement placement;
    placement.size = Size2{200, 200};
    placement.horizontal = UI::Align::Center;
    placement.vertical = UI::Align::Center;
    panel->setPlacement(placement);
    panel->layout() = UI::Layout{UI::Flow::Column, 10, 5, UI::Align::Start};
    screen->add(panel);
    auto scaled = std::make_shared<UI::Button>();
    placement = UI::Placement{};
    placement.size = Size2{50, 20};
    placement.horizontal = UI::Align::End;
    scaled->setPlacement(placement);
    auto fixed = std::make_shared<UI::Button>();
    placement.scaleMode = UI::ScaleMode::Pixels;
    fixed->setPlacement(placement);
    panel->add(scaled);
    panel->add(fixed);
    context.setScreen(screen);
    require(context.bounds(*panel) == Rect{200, 100, 400, 400}, "Panel alignment failed");
    require(context.bounds(*scaled) == Rect{480, 120, 100, 40}, "Scaled layout failed");
    require(context.bounds(*fixed) == Rect{530, 170, 50, 20}, "Pixel element failed to align independently");
    context.setViewport(Viewport{0, 0, 1000, 800}, 1.5F);
    require(context.bounds(*fixed)->size == Size2{50, 20}, "Pixel element resized with UI");
    require(near(context.bounds(*fixed)->position.x, 585), "Pixel element did not realign");
    require(!panel->add(panel) && !panel->add(screen) && !screen->add(fixed), "Invalid ownership accepted");
    int clicks = 0;
    auto subscription = fixed->subscribe(UI::EventType::Click, [&](const UI::Event&) { ++clicks; });
    const auto point = context.bounds(*fixed)->center();
    click(context, static_cast<int>(point.x), static_cast<int>(point.y));
    require(clicks == 1, "Hit geometry disagrees with layout");
    placement.offset = Vector2{0, 500};
    fixed->setPlacement(placement);
    const auto outside = context.bounds(*fixed)->center();
    click(context, static_cast<int>(outside.x), static_cast<int>(outside.y));
    require(clicks == 1, "Clipped element accepted input");
    require(!context.setViewport(Viewport{0, 0, 0, 0}), "Empty viewport accepted");
    require(!context.handle(mouse(NeuexpEngine::EventType::MousePressed, 10, 10)), "Invalid viewport accepted input");
}
void scrolling()
{
    UI::Context context;
    context.setViewport(Viewport{0, 0, 200, 100});
    auto screen = std::make_shared<UI::Screen>();
    screen->layout() = UI::Layout{UI::Flow::Column, 10, 0, UI::Align::Center};
    screen->setScrollable(true);
    screen->setPointerPolicy(UI::PointerPolicy::Block);
    std::vector<std::shared_ptr<UI::Button>> buttons;
    for (int index = 0; index < 4; ++index) {
        auto button = std::make_shared<UI::Button>();
        UI::Placement placement;
        placement.size = Size2{100, 40};
        button->setPlacement(placement);
        screen->add(button);
        buttons.push_back(button);
    }
    context.setScreen(screen);
    int clicks = 0;
    auto subscription = buttons.back()->subscribe(UI::EventType::Click, [&](const UI::Event&) { ++clicks; });
    auto event = mouse(NeuexpEngine::EventType::MouseWheel, 20, 50);
    event.wheelY = -10;
    require(context.handle(event), "Scroll not consumed");
    require(near(screen->scrollOffset(), 80), "Scroll not clamped to content");
    click(context, 20, 70);
    require(clicks == 1, "Scrolled element not clickable");
}
void construction()
{
    auto screen = std::make_shared<UI::Screen>();
    const UI::Placement placement = UI::Placement::horizontalCenter(Size2{120, 24});
    auto label = screen->addLabel(placement, "Text");
    require(label != nullptr, "Label construction returned null");
    require(screen->children().size() == 1, "Label construction did not add child");
    require(near(label->placement().size.width, 120), "Label construction lost width");
    require(near(label->placement().size.height, 24), "Label construction lost height");
    require(label->placement().horizontal == UI::Align::Center, "Label construction lost alignment");

    UI::Placement indicatorPlacement;
    indicatorPlacement.size = Size2{80, 20};
    auto indicator = screen->addNumericIndicator(indicatorPlacement);
    require(indicator != nullptr, "Indicator construction returned null");
    require(screen->addButton(placement, "Button") != nullptr, "Button construction failed");
    require(screen->addCheckbox(placement, "Check") != nullptr, "Checkbox construction failed");
    require(screen->addProgressBar(placement) != nullptr, "Progress bar construction failed");
    require(screen->addContainer(placement) != nullptr, "Container construction failed");
    require(screen->children().size() == 6, "Construction methods did not add children");
}
void textMetrics()
{
    require(Renderer2D::measureDebugText("A", 1) == Size2{5, 7}, "Latin glyph metrics incorrect");
    require(Renderer2D::measureDebugText("A0", 1) == Size2{11, 7}, "Latin/digit metrics differ from Cyrillic");
    require(Renderer2D::measureDebugText("a0", 1) == Renderer2D::measureDebugText("A0", 1), "Latin case metrics differ");
    require(Renderer2D::measureDebugText(u8"Яё", 1) == Size2{11, 7}, "UTF-8 counted as bytes");
    require(Renderer2D::measureDebugText(u8"Я\nё", 2) == Size2{10, 30}, "Multiline metrics incorrect");
    require(Renderer2D::measureDebugText("") == Size2{}, "Empty text has size");
}
}
int main()
{
    try {
        routing();
        mutation();
        subscriptions();
        actionSubscription();
        layout();
        scrolling();
        construction();
        textMetrics();
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
