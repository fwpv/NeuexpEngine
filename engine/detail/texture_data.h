#pragma once

#include "texture.h"

namespace Engine
{
class Renderer2D;

namespace Detail
{
struct TextureData
{
    const Renderer2D* owner = nullptr;
    unsigned int id = 0;
    int width = 0;
    int height = 0;
    int storageWidth = 0;
    int storageHeight = 0;
    std::size_t gpuBytes = 0;
    std::unique_ptr<Image> image;
    std::filesystem::path path;
};
}
}
