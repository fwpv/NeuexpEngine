#include "platform.h"
#include "platform_input.h"
#include "input.h"

#include <chrono>
#include <utility>
#include <vector>

namespace Platform
{
namespace
{
Engine::InputState input;
std::vector<Engine::Event> pendingEvents;
}

const Engine::InputState& getInput()
{
    return input;
}

const std::vector<Engine::Event>& getEvents()
{
    return input.events();
}

bool isKeyDown(Key key)
{
    return input.key(key).down;
}

bool isKeyPressed(Key key)
{
    return input.key(key).pressed;
}

bool isKeyReleased(Key key)
{
    return input.key(key).released;
}

namespace Detail
{
void queueEvent(Engine::Event event)
{
    event.time = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    pendingEvents.push_back(std::move(event));
}

void queueEvent(Engine::EventType type)
{
    Engine::Event event;
    event.type = type;
    queueEvent(std::move(event));
}

void finishInputFrame()
{
    input.beginFrame();
    for (auto& event : pendingEvents) {
        input.handleEvent(std::move(event));
    }
    pendingEvents.clear();
}

void resetInput()
{
    input = {};
    pendingEvents.clear();
}
}
}
