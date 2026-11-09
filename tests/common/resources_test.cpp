#include "resources.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using namespace Engine;

void require(bool success, const std::string& message)
{
    if (!success) {
        throw std::runtime_error(message);
    }
}

void check(bool success, const ResourceError& error)
{
    require(success && error.code == ResourceErrorCode::None, error.message);
}

void run()
{
    ResourceError error;
    ResourceFiles files;
    std::vector<std::uint8_t> bytes = { 42 };
    require(!files.readBinary("x", bytes, error) && error.code == ResourceErrorCode::InvalidState,
        "Uninitialized file system accepted a read.");
    require(!files.initialize(error, "relative"), "Relative resource root accepted.");
    check(files.initialize(error), error);
    std::filesystem::path executable;
    check(executableDirectory(executable, error), error);
    require(executable.is_absolute(), "Executable directory is not absolute.");
    const auto original = std::filesystem::current_path();
    std::filesystem::current_path(std::filesystem::temp_directory_path());
    try {
        std::filesystem::path afterDirectoryChange;
        check(executableDirectory(afterDirectoryChange, error), error);
        require(afterDirectoryChange == executable, "Executable directory depends on cwd.");
        ResourceFiles alternate;
        check(alternate.initialize(error, std::filesystem::temp_directory_path()), error);
        check(executableDirectory(afterDirectoryChange, error), error);
        require(afterDirectoryChange == executable, "Executable directory depends on asset root.");
        check(files.readBinary("raw_3x2.rgba", bytes, error), error);
        require(bytes.size() == 24 && bytes.front() == 255 && bytes.back() == 128, "Raw RGBA changed.");
        std::string text;
        check(files.readText("resource_text.txt", text, error), error);
        const std::string expected = std::string(u8"Ресурсы\r\nText") + '\0' + "tail\n";
        require(text == expected, "Text bytes, UTF-8, CRLF or embedded null changed.");
        const auto preserved = bytes;
        require(!files.readBinary("missing.png", bytes, error) && error.code == ResourceErrorCode::Io
            && !error.message.empty() && bytes == preserved, "Missing-file contract failed.");
        require(!files.readBinary("raw_3x2.rgba", bytes, error, 23)
            && error.code == ResourceErrorCode::LimitExceeded && bytes == preserved, "File limit ignored.");
        std::filesystem::path resolved;
        check(files.resolve("./rgb_3x2.png", resolved, error), error);
        const auto first = resolved;
        check(files.resolve("rgb_3x2.png", resolved, error), error);
        require(first == resolved, "Resource aliases did not normalize.");
        for (const auto& name : { std::string("../outside"), files.root().u8string(), std::string(""), std::string("a\0b", 3) }) {
            require(!files.resolve(name, resolved, error) && error.code == ResourceErrorCode::InvalidPath,
                "Invalid resource path accepted.");
        }
        Image image;
        check(files.loadPng("rgba_2x2.png", image, error), error);
        const std::vector<std::uint8_t> expectedPixels = { 255,0,0,0, 0,255,0,128, 0,0,255,255, 255,255,255,64 };
        require(image.width == 2 && image.height == 2 && image.pixels == expectedPixels,
            "RGBA orientation, colors or alpha changed.");
        check(files.readBinary("rgba_2x2.png", bytes, error), error);
        Image fromMemory;
        check(decodePng(bytes, fromMemory, error), error);
        require(fromMemory.pixels == image.pixels, "File/memory decoding differs.");
        ResourceLimits limits;
        limits.imageDimension = 1;
        require(!decodePng(bytes, image, error, limits) && error.code == ResourceErrorCode::LimitExceeded
            && image.pixels == expectedPixels, "Dimension limit or output preservation failed.");
        limits = {};
        limits.imageBytes = 15;
        require(!decodePng(bytes, image, error, limits) && error.code == ResourceErrorCode::LimitExceeded,
            "RGBA memory limit ignored.");
        limits = {};
        limits.decoderBytes = 1;
        require(!decodePng(bytes, image, error, limits) && error.code == ResourceErrorCode::LimitExceeded,
            "Temporary decoder memory limit ignored.");
        limits = {};
        limits.fileBytes = bytes.size() - 1;
        require(!decodePng(bytes, image, error, limits) && error.code == ResourceErrorCode::LimitExceeded,
            "Encoded memory limit ignored.");
        require(!files.loadPng("truncated.png", image, error) && error.code == ResourceErrorCode::InvalidImage
            && image.pixels == expectedPixels, "Corrupt PNG accepted or output overwritten.");
        require(!decodePng({}, image, error) && !decodePng({ 1,2,3 }, image, error), "Non-PNG input accepted.");
        check(files.loadPng("rgb_3x2.png", image, error), error);
        require(image.width == 3 && image.height == 2 && image.pixels.size() == 24
            && image.pixels[3] == 255 && image.pixels[23] == 255, "RGB was not converted to RGBA8.");
        check(files.loadPng("gray16.png", image, error), error);
        require(image.pixels == std::vector<std::uint8_t>({ 0x12,0x12,0x12,255,255,255,255,255 }),
            "16-bit grayscale was not converted to RGBA8.");
        require(!validateImage({ 0,1,{} }, error) && !validateImage({ 1,1,{1,2,3} }, error), "Invalid RGBA accepted.");
        require(!validateImage({ std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), {} }, error)
            && error.code == ResourceErrorCode::LimitExceeded, "Large dimensions accepted.");
        // Проверяется путь с Unicode, пустой файл и фиксированная явно заданная база.
        const auto unicode = files.root() / std::filesystem::u8path(u8"ресурс.png");
        std::filesystem::copy_file(files.root() / "rgba_2x2.png", unicode, std::filesystem::copy_options::overwrite_existing);
        check(files.loadPng(u8"ресурс.png", image, error), error);
        std::filesystem::remove(unicode);
        const auto empty = files.root() / "empty.txt";
        { std::ofstream output(empty, std::ios::binary); }
        check(files.readBinary("empty.txt", bytes, error, 0), error);
        require(bytes.empty(), "Empty file was not read.");
        std::filesystem::remove(empty);
        ResourceFiles explicitRoot;
        check(explicitRoot.initialize(error, files.root()), error);
        check(explicitRoot.readText("resource_text.txt", text, error), error);
        require(text == expected, "Explicit root changed after chdir.");
    } catch (...) {
        std::filesystem::current_path(original);
        throw;
    }
    std::filesystem::current_path(original);
}
}

int main()
{
    try {
        run();
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
