#pragma once

#include "input.h"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace NeuexpEngine
{
namespace Detail
{
struct EventSlot;
}

// Уничтожение подписки и disconnect отключают обработчик.
// Подписка перемещается, не копируется и может пережить диспетчер.
class EventSubscription
{
public:
    EventSubscription() = default;
    ~EventSubscription();
    EventSubscription(EventSubscription&& other) noexcept;
    EventSubscription& operator=(EventSubscription&& other) noexcept;
    EventSubscription(const EventSubscription&) = delete;
    EventSubscription& operator=(const EventSubscription&) = delete;

    void disconnect() noexcept;
    bool connected() const noexcept;

private:
    friend class EventDispatcher;
    explicit EventSubscription(const std::shared_ptr<Detail::EventSlot>& slot);
    std::weak_ptr<Detail::EventSlot> slot;
};

// Необязательная синхронная рассылка уже сформированных событий в одном потоке.
// Диспетчер не опрашивает ОС и не изменяет InputState. Обработчики вызываются
// в порядке регистрации; остановку распространения и маршрутизацию решает приложение.
class EventDispatcher
{
public:
    using Handler = std::function<void(const Event&)>;

    EventDispatcher() = default;
    ~EventDispatcher();
    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;
    EventDispatcher(EventDispatcher&&) = delete;
    EventDispatcher& operator=(EventDispatcher&&) = delete;

    // Уничтожение возвращённого объекта отключает обработчик. Пустой Handler даёт пустую подписку.
    [[nodiscard]] EventSubscription subscribe(Handler handler);
    [[nodiscard]] EventSubscription subscribe(EventType type, Handler handler);

    // Событие должно оставаться действительным до возврата. Новые подписки
    // не входят в текущую рассылку, отключённые пропускаются немедленно.
    // Вложенная рассылка синхронна и имеет собственный снимок подписок.
    // Исключение обработчика передаётся вызывающему; оставшиеся не вызываются.
    void dispatch(const Event& event);

private:
    EventSubscription add(std::optional<EventType> type, Handler handler);
    void removeDisconnected();
    std::vector<std::shared_ptr<Detail::EventSlot>> slots;
};
}
