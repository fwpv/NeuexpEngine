#pragma once

#include "app/input.h"

// Входящие события сохраняются до публикации снимка в finishInputFrame().
namespace Platform::Detail
{
void queueEvent(NeuexpEngine::Event event);
void queueEvent(NeuexpEngine::EventType type);
void finishInputFrame();
void resetInput();
}
