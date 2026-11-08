#pragma once

#include <functional>
#include <memory>
#include <vector>

namespace Engine::Gui
{
enum class EventType { Enter, Leave, Press, Release, Click, Change, Cancel };

struct Event
{
    EventType type;
    bool checked = false;
};

namespace Detail { struct Slot; }

// Подписку хранит получатель. При захвате this она должна уничтожаться раньше получателя.
class Subscription
{
public:
    Subscription() = default;
    ~Subscription();
    Subscription(Subscription&& other) noexcept;
    Subscription& operator=(Subscription&& other) noexcept;
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;
    void disconnect();
    bool connected() const;

private:
    friend class Events;
    explicit Subscription(const std::shared_ptr<Detail::Slot>& slot);
    std::weak_ptr<Detail::Slot> slot;
};

class Events
{
public:
    using Handler = std::function<void(const Event&)>;
    Events() = default;
    ~Events();
    Events(const Events&) = delete;
    Events& operator=(const Events&) = delete;
    [[nodiscard]] Subscription subscribe(EventType type, Handler handler);
    // Снимок подписок: добавленные ждут следующей рассылки, отключённые пропускаются.
    void emit(const Event& event);

private:
    std::vector<std::shared_ptr<Detail::Slot>> slots;
};
}
