#pragma once

#include "platform.h"

namespace Platform
{
bool validateWindowTitle(const std::string& title, const char* operation, Error& error);
bool validateWindowOptions(const WindowOptions& options, Error& error);
}
