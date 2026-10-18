#include "keyboard_query_counter.h"

#include <X11/Xlib.h>

namespace
{
unsigned int queryCount = 0;
}

extern "C" int __real_XQueryKeymap(Display* display, char keys[32]);

// --wrap оставляет настоящий запрос, добавляя только наблюдение за числом вызовов.
extern "C" int __wrap_XQueryKeymap(Display* display, char keys[32])
{
    ++queryCount;
    return __real_XQueryKeymap(display, keys);
}

unsigned int keyboardQueryCount()
{
    return queryCount;
}
