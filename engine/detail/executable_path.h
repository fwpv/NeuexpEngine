#pragma once

#include "resources.h"

namespace Engine::Detail
{
bool executableDirectory(std::filesystem::path& result, ResourceError& error);
}
