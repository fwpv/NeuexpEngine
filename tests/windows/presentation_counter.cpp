#include "presentation_counter.h"

#include <windows.h>

namespace
{
unsigned int swapCount = 0;
}

namespace Platform::Testing
{
BOOL swapWindowBuffers(HDC deviceContext)
{
    ++swapCount;
    return SwapBuffers(deviceContext);
}
}

unsigned int presentationCount()
{
    return swapCount;
}
