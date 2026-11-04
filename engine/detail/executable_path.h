#pragma once

#include "resources.h"

#include <filesystem>

namespace Engine::Detail
{
bool executableDirectory(std::filesystem::path& result, ResourceError& error);
}
