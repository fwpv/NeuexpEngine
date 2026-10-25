// Макрос генерирует реализацию библиотеки строго в одной единице трансляции.
#include "decode_memory.h"
#define STBI_MALLOC(size) ImageDecoderMemory::allocate(size)
#define STBI_REALLOC(pointer, size) ImageDecoderMemory::resize(pointer, size)
#define STBI_FREE(pointer) ImageDecoderMemory::release(pointer)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
