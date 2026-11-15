#include "events.h"
#include "input.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testDelivery()
{
    NeuexpEngine::EventDispatcher dispatcher;
    NeuexpEngine::Event event;
    event.type = NeuexpEngine::EventType::TextInput;
    event.text = u8"Текст";
    event.time = 12.5;
    dispatcher.dispatch(event);
    std::vector<int> calls;
    auto first = dispatcher.subscribe([&](const NeuexpEngine::Event& received) {
        require(&received == &event, "Event must be delivered without replacement");
        require(received.text == u8"Текст" && received.time == 12.5, "Payload changed");
        calls.push_back(1);
    });
    auto filtered = dispatcher.subscribe(NeuexpEngine::EventType::TextInput, [&](const NeuexpEngine::Event&) {
        calls.push_back(2);
    });
    auto other = dispatcher.subscribe(NeuexpEngine::EventType::KeyPressed, [&](const NeuexpEngine::Event&) {
        calls.push_back(3);
    });
    auto last = dispatcher.subscribe([&](const NeuexpEngine::Event&) { calls.push_back(4); });
    dispatcher.dispatch(event);
    require(calls == std::vector<int>({1, 2, 4}), "Registration order or type filter");
    calls.clear();
    event.type = NeuexpEngine::EventType::KeyPressed;
    dispatcher.dispatch(event);
    require(calls == std::vector<int>({1, 3, 4}), "All-events subscription");
    auto empty = dispatcher.subscribe({});
    auto emptyFiltered = dispatcher.subscribe(NeuexpEngine::EventType::TextInput, {});
    require(!empty.connected() && !emptyFiltered.connected(), "Empty handler");
    NeuexpEngine::EventDispatcher independent;
    independent.dispatch(event);
    require(calls.size() == 3, "Dispatchers must be independent");
}

void testOwnership()
{
    static_assert(!std::is_copy_constructible<NeuexpEngine::EventSubscription>::value);
    static_assert(std::is_nothrow_move_constructible<NeuexpEngine::EventSubscription>::value);
    static_assert(!std::is_move_constructible<NeuexpEngine::EventDispatcher>::value);
    NeuexpEngine::EventSubscription survivor;
    survivor.disconnect();
    survivor.disconnect();
    require(!survivor.connected(), "Default subscription");
    int calls = 0;
    {
        NeuexpEngine::EventDispatcher dispatcher;
        {
            auto scoped = dispatcher.subscribe([&](const NeuexpEngine::Event&) { ++calls; });
            require(scoped.connected(), "Subscription must be connected");
            auto moved = std::move(scoped);
            require(!scoped.connected() && moved.connected(), "Move constructor");
            survivor = dispatcher.subscribe([&](const NeuexpEngine::Event&) { calls += 100; });
            survivor = std::move(moved);
            require(!moved.connected() && survivor.connected(), "Move assignment");
            auto& alias = survivor;
            survivor = std::move(alias);
            dispatcher.dispatch({});
            require(calls == 1, "Move assignment must disconnect previous handler");
        }
        {
            auto scoped = dispatcher.subscribe([&](const NeuexpEngine::Event&) { calls += 100; });
        }
        dispatcher.dispatch({});
        require(calls == 2, "Scope exit must disconnect");
    }
    require(!survivor.connected(), "Subscription outlives dispatcher");
    survivor.disconnect();

    NeuexpEngine::EventDispatcher dispatcher;
    auto resource = std::make_shared<int>(1);
    std::weak_ptr<int> weakResource = resource;
    auto subscription = dispatcher.subscribe([resource](const NeuexpEngine::Event&) {});
    resource.reset();
    require(!weakResource.expired(), "Handler capture lifetime");
    subscription.disconnect();
    require(weakResource.expired(), "Disconnect must release captures immediately");
    subscription.disconnect();
}

void testMutation()
{
    NeuexpEngine::EventDispatcher dispatcher;
    std::vector<int> calls;
    NeuexpEngine::EventSubscription self;
    NeuexpEngine::EventSubscription next;
    NeuexpEngine::EventSubscription added;
    self = dispatcher.subscribe([&, localCalls = 0](const NeuexpEngine::Event&) mutable {
        ++localCalls;
        calls.push_back(localCalls);
        self.disconnect();
        next.disconnect();
        added = dispatcher.subscribe([&](const NeuexpEngine::Event&) { calls.push_back(4); });
        // Захваты выполняемой функции живы после отключения самой функции.
        calls.push_back(localCalls + 1);
    });
    next = dispatcher.subscribe([&](const NeuexpEngine::Event&) { calls.push_back(99); });
    auto last = dispatcher.subscribe([&](const NeuexpEngine::Event&) { calls.push_back(3); });
    dispatcher.dispatch({});
    require(calls == std::vector<int>({1, 2, 3}), "Mutation during dispatch");
    calls.clear();
    dispatcher.dispatch({});
    require(calls == std::vector<int>({3, 4}), "New subscription begins next dispatch");

    int state = 0;
    auto mutableHandler = dispatcher.subscribe([count = 0, &state](const NeuexpEngine::Event&) mutable {
        state = ++count;
    });
    dispatcher.dispatch({});
    dispatcher.dispatch({});
    require(state == 2, "Mutable callback state must persist");

    // Перераспределение хранилища при массовой регистрации не портит текущую рассылку.
    std::vector<NeuexpEngine::EventSubscription> many;
    constexpr int SubscriptionCount = 256;
    int addedCalls = 0;
    auto grow = dispatcher.subscribe([&](const NeuexpEngine::Event&) {
        if (many.empty()) {
            for (int index = 0; index < SubscriptionCount; ++index) {
                many.push_back(dispatcher.subscribe([&](const NeuexpEngine::Event&) { ++addedCalls; }));
            }
        }
    });
    dispatcher.dispatch({});
    require(addedCalls == 0, "Added subscriptions ran in current dispatch");
    dispatcher.dispatch({});
    require(addedCalls == SubscriptionCount, "Missing subscriptions after growth");
    many.clear();
    dispatcher.dispatch({});
    require(addedCalls == SubscriptionCount, "Destroyed subscriptions still called");
}

void testNestedAndExceptions()
{
    NeuexpEngine::EventDispatcher dispatcher;
    std::vector<int> calls;
    NeuexpEngine::EventSubscription added;
    auto first = dispatcher.subscribe([&](const NeuexpEngine::Event& event) {
        calls.push_back(1);
        if (event.type == NeuexpEngine::EventType::KeyPressed) {
            added = dispatcher.subscribe([&](const NeuexpEngine::Event&) { calls.push_back(3); });
            NeuexpEngine::Event nested;
            nested.type = NeuexpEngine::EventType::KeyReleased;
            dispatcher.dispatch(nested);
        }
    });
    auto second = dispatcher.subscribe([&](const NeuexpEngine::Event&) { calls.push_back(2); });
    NeuexpEngine::Event event;
    event.type = NeuexpEngine::EventType::KeyPressed;
    dispatcher.dispatch(event);
    require(calls == std::vector<int>({1, 1, 2, 3, 2}), "Nested dispatch order");

    NeuexpEngine::EventDispatcher failing;
    auto throwing = failing.subscribe([](const NeuexpEngine::Event&) { throw std::runtime_error("handler"); });
    int delivered = 0;
    auto following = failing.subscribe([&](const NeuexpEngine::Event&) { ++delivered; });
    bool caught = false;
    try {
        failing.dispatch(event);
    } catch (const std::runtime_error&) {
        caught = true;
    }
    require(caught && delivered == 0, "Handler exception contract");
    throwing.disconnect();
    failing.dispatch(event);
    require(delivered == 1, "Dispatcher must remain usable after exception");

    auto destroyed = std::make_unique<NeuexpEngine::EventDispatcher>();
    auto destroyer = destroyed->subscribe([&](const NeuexpEngine::Event&) { destroyed.reset(); });
    auto skipped = destroyed->subscribe([&](const NeuexpEngine::Event&) { ++delivered; });
    destroyed->dispatch(event);
    require(delivered == 1 && !destroyer.connected() && !skipped.connected(),
        "Dispatcher destroyed during callback");
}

void testInputIntegration()
{
    NeuexpEngine::InputState input;
    NeuexpEngine::Event event;
    event.type = NeuexpEngine::EventType::KeyPressed;
    event.key = NeuexpEngine::Key::A;
    input.handleEvent(event);
    event.type = NeuexpEngine::EventType::FocusLost;
    input.handleEvent(event);
    NeuexpEngine::EventDispatcher dispatcher;
    std::vector<NeuexpEngine::EventType> types;
    auto listener = dispatcher.subscribe([&](const NeuexpEngine::Event& received) {
        types.push_back(received.type);
        if (received.type == NeuexpEngine::EventType::KeyReleased) {
            require(received.canceled && received.key == NeuexpEngine::Key::A, "Canceled release payload");
        }
    });
    for (const auto& received : input.events()) {
        dispatcher.dispatch(received);
    }
    require(types == std::vector<NeuexpEngine::EventType>({NeuexpEngine::EventType::KeyPressed,
        NeuexpEngine::EventType::FocusLost, NeuexpEngine::EventType::KeyReleased}), "Input event order");
    require(input.events().size() == 3 && !input.key(NeuexpEngine::Key::A).down,
        "Dispatch must not consume events or modify input");
}
}

int main()
{
    try {
        testDelivery();
        testOwnership();
        testMutation();
        testNestedAndExceptions();
        testInputIntegration();
        std::cout << "Event dispatcher tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
