#include "events.h"

#include <algorithm>
#include <utility>

namespace NeuexpEngine::Gui
{
namespace Detail
{
struct Slot
{
    EventType type;
    std::shared_ptr<Events::Handler> handler;
};
}

Subscription::Subscription(const std::shared_ptr<Detail::Slot>& slot)
    : slot(slot)
{
}
Subscription::~Subscription()
{
    disconnect();
}
Subscription::Subscription(Subscription&& other) noexcept
    : slot(std::move(other.slot))
{
}
Subscription& Subscription::operator=(Subscription&& other) noexcept
{
    if (this != &other) {
        disconnect();
        slot = std::move(other.slot);
    }
    return *this;
}
void Subscription::disconnect()
{
    if (const auto current = slot.lock()) {
        current->handler.reset();
    }
}
bool Subscription::connected() const
{
    const auto current = slot.lock();
    return current && static_cast<bool>(current->handler);
}
Events::~Events()
{
    for (const auto& slot : slots) {
        slot->handler.reset();
    }
}
Subscription Events::subscribe(EventType type, Handler handler)
{
    slots.erase(std::remove_if(slots.begin(), slots.end(), [](const auto& item) {
        return !item->handler;
    }), slots.end());
    if (!handler) {
        return {};
    }
    auto slot = std::make_shared<Detail::Slot>();
    slot->type = type;
    slot->handler = std::make_shared<Handler>(std::move(handler));
    slots.push_back(slot);
    return Subscription(slot);
}
void Events::emit(const Event& event)
{
    const auto snapshot = slots;
    for (const auto& slot : snapshot) {
        const auto handler = slot->handler;
        if (handler && slot->type == event.type) {
            (*handler)(event);
        }
    }
}
}
