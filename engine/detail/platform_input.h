#pragma once

#include "input.h"

// Внутренний мост платформ: сообщения Win32 могут приходить и вне processEvents.
// Поэтому сначала сохраняем их, а снимок публикуем только при завершении processEvents.
namespace Platform::Detail
{
void queueEvent(Engine::Event event);
void queueEvent(Engine::EventType type);
void finishInputFrame();
void resetInput();
}
