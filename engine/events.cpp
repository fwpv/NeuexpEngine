#include "events.h"

#include <algorithm>
#include <utility>

namespace Engine
{
namespace Detail
{
struct EventSlot
{
    std::optional<EventType> type;
    std::shared_ptr<EventDispatcher::Handler> handler;
};
}

EventSubscription::EventSubscription(const std::shared_ptr<Detail::EventSlot>& slot)
    : slot(slot)
{
}

EventSubscription::~EventSubscription()
{
    disconnect();
}

EventSubscription::EventSubscription(EventSubscription&& other) noexcept
    : slot(std::move(other.slot))
{
}

EventSubscription& EventSubscription::operator=(EventSubscription&& other) noexcept
{
    if (this != &other) {
        disconnect();
        slot = std::move(other.slot);
    }
    return *this;
}

void EventSubscription::disconnect() noexcept
{
    if (const auto current = slot.lock()) {
        current->handler.reset();
    }
    slot.reset();
}

bool EventSubscription::connected() const noexcept
{
    const auto current = slot.lock();
    return current && static_cast<bool>(current->handler);
}

EventDispatcher::~EventDispatcher()
{
    for (const auto& slot : slots) {
        slot->handler.reset();
    }
}

EventSubscription EventDispatcher::subscribe(Handler handler)
{
    return add(std::nullopt, std::move(handler));
}

EventSubscription EventDispatcher::subscribe(EventType type, Handler handler)
{
    return add(type, std::move(handler));
}

EventSubscription EventDispatcher::add(std::optional<EventType> type, Handler handler)
{
    removeDisconnected();
    if (!handler) {
        return {};
    }
    auto slot = std::make_shared<Detail::EventSlot>();
    slot->type = type;
    slot->handler = std::make_shared<Handler>(std::move(handler));
    slots.push_back(slot);
    return EventSubscription(slot);
}

void EventDispatcher::removeDisconnected()
{
    slots.erase(std::remove_if(slots.begin(), slots.end(), [](const auto& slot) {
        return !slot->handler;
    }), slots.end());
}

void EventDispatcher::dispatch(const Event& event)
{
    removeDisconnected();
    // Копируются только shared_ptr, не функции: изменяемое состояние лямбды
    // сохраняется между вызовами. Снимок устойчив к добавлению и удалению подписок.
    const auto currentSlots = slots;
    for (const auto& slot : currentSlots) {
        if (!slot->type || *slot->type == event.type) {
            // Локальное владение сохраняет выполняемый обработчик, даже если
            // он отключит собственную подписку или уничтожит диспетчер.
            const auto handler = slot->handler;
            if (handler) {
                (*handler)(event);
            }
        }
    }
}
}
