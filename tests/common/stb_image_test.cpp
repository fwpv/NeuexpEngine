#include <stb_image.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

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

bool checkLoadFailure(const char* path, bool shouldExist)
{
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error || exists != shouldExist) {
        std::cerr << "Unexpected test file availability: " << path << "\n";
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(path, &width, &height, &channels, ImageChannels);
    if (pixels) {
        std::cerr << "Loading should have failed: " << path << "\n";
        stbi_image_free(pixels);
        return false;
    }
    return true;
}
}

int main(int argumentCount, char* arguments[])
{
    if (argumentCount != 3) {
        std::cerr << "Usage: stb_image_test <pixels|missing|corrupt> <file>\n";
        return 1;
    }

    const std::string scenario = arguments[1];
    bool passed = false;
    if (scenario == "pixels") {
        passed = checkPixels(arguments[2]);
    } else if (scenario == "missing") {
        passed = checkLoadFailure(arguments[2], false);
    } else if (scenario == "corrupt") {
        passed = checkLoadFailure(arguments[2], true);
    } else {
        std::cerr << "Unknown scenario: " << arguments[1] << "\n";
    }

    return passed ? 0 : 1;
}
