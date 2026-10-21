#pragma once

#include "input.h"

// Входящие события сохраняются до публикации снимка в finishInputFrame().
namespace Platform::Detail
{
void queueEvent(Engine::Event event);
void queueEvent(Engine::EventType type);
void finishInputFrame();
void resetInput();
}
