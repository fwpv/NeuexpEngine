#include "presentation_counter.h"

#include <GL/glx.h>

namespace
{
unsigned int swapCount = 0;
}

extern "C" void __real_glXSwapBuffers(Display* display, GLXDrawable drawable);

// --wrap сохраняет настоящий обмен буферов и считает обращения к нему.
extern "C" void __wrap_glXSwapBuffers(Display* display, GLXDrawable drawable)
{
    ++swapCount;
    __real_glXSwapBuffers(display, drawable);
}

unsigned int presentationCount()
{
    return swapCount;
}
