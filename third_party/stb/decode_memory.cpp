#include "decode_memory.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

namespace ImageDecoderMemory
{
namespace
{
thread_local Budget* active = nullptr;

struct alignas(std::max_align_t) Header
{
    std::size_t bytes;
    Budget* budget;
};
}

Budget::Budget(std::size_t limit)
    : limit(limit)
    , previous(active)
{
    active = this;
}

Budget::~Budget()
{
    active = previous;
}

bool Budget::exceeded() const
{
    return failed;
}

void* allocate(std::size_t bytes)
{
    if (active && (bytes > active->limit || active->used > active->limit - bytes)) {
        active->failed = true;
        return nullptr;
    }
    if (bytes > std::numeric_limits<std::size_t>::max() - sizeof(Header)) {
        return nullptr;
    }
    void* storage = std::malloc(sizeof(Header) + bytes);
    if (!storage) { return nullptr; }
    auto* header = new (storage) Header{ bytes, active };
    if (active) { active->used += bytes; }
    return header + 1;
}

void release(void* pointer)
{
    if (!pointer) { return; }
    auto* header = static_cast<Header*>(pointer) - 1;
    if (header->budget) { header->budget->used -= header->bytes; }
    std::free(header);
}

void* resize(void* pointer, std::size_t bytes)
{
    if (!pointer) { return allocate(bytes); }
    if (bytes == 0) { release(pointer); return nullptr; }
    auto* header = static_cast<Header*>(pointer) - 1;
    void* replacement = allocate(bytes);
    if (!replacement) { return nullptr; }
    std::memcpy(replacement, pointer, std::min(bytes, header->bytes));
    release(pointer);
    return replacement;
}
}
