#pragma once

#include <X11/Xlib.h>
#include <GL/glx.h>

namespace Platform::Testing
{
const char* queryVSyncExtensions(Display* display, int screen);
__GLXextFuncPtr lookupVSyncFunction(const GLubyte* name);
Bool directVSyncContext(Display* display, GLXContext context);
}

#define glXQueryExtensionsString Platform::Testing::queryVSyncExtensions
#define glXGetProcAddressARB Platform::Testing::lookupVSyncFunction
#define glXIsDirect Platform::Testing::directVSyncContext
