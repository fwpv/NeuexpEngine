#pragma once

#include <cstdint>

namespace NeuexpEngine
{
// Уникален внутри сцены и не переиспользуется, в том числе после clear().
// Ноль означает, что объект не принадлежит сцене.
using ObjectId = std::uint64_t;
}
