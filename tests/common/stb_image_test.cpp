#include <stb_image.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <string>

namespace
{
constexpr int ImageWidth = 2;
constexpr int ImageHeight = 2;
constexpr int ImageChannels = 4;
constexpr std::array<unsigned char, ImageWidth * ImageHeight * ImageChannels> ExpectedPixels = {
    255, 0, 0, 0,       0, 255, 0, 128,
    0, 0, 255, 255,     255, 255, 255, 64
};

bool checkPixels(const char* path)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(path, &width, &height, &channels, ImageChannels);
    if (!pixels) {
        std::cerr << "Cannot load PNG: " << path << "\n";
        return false;
    }

    bool passed = width == ImageWidth && height == ImageHeight && channels == ImageChannels;
    if (!passed) {
        std::cerr << "Expected 2x2 RGBA PNG, got " << width << "x" << height
                  << " with " << channels << " channels\n";
    } else {
        passed = std::equal(ExpectedPixels.begin(), ExpectedPixels.end(), pixels);
        if (!passed) {
            std::cerr << "Decoded PNG colors or alpha values differ from expected pixels\n";
        }
    }

    stbi_image_free(pixels);
    return passed;
}

}

int main(int argumentCount, char* arguments[])
{
    if (argumentCount != 3) {
        std::cerr << "Usage: stb_image_test <pixels> <file>\n";
        return 1;
    }

    const std::string scenario = arguments[1];
    bool passed = false;
    if (scenario == "pixels") {
        passed = checkPixels(arguments[2]);
    } else {
        std::cerr << "Unknown scenario: " << arguments[1] << "\n";
    }

    return passed ? 0 : 1;
}
