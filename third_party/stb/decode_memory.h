#pragma once

#include <cstddef>

namespace ImageDecoderMemory
{
// Ограничение всех временных выделений одного синхронного декодирования.
// Все выделенные при действующем бюджете блоки освобождаются до его уничтожения.
class Budget
{
public:
    explicit Budget(std::size_t limit);
    ~Budget();
    Budget(const Budget&) = delete;
    Budget& operator=(const Budget&) = delete;
    bool exceeded() const;

private:
    std::size_t limit;
    std::size_t used = 0;
    bool failed = false;
    Budget* previous = nullptr;
    friend void* allocate(std::size_t bytes);
    friend void release(void* pointer);
};

void* allocate(std::size_t bytes);
void* resize(void* pointer, std::size_t bytes);
void release(void* pointer);
}
