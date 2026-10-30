#include "resources.h"

#include <stb_image.h>
#include <decode_memory.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <new>
#include <utility>

namespace Engine
{
namespace
{
std::uint32_t bigEndian(const std::uint8_t* bytes)
{
    return (static_cast<std::uint32_t>(bytes[0]) << 24) | (static_cast<std::uint32_t>(bytes[1]) << 16)
        | (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
}

// stb_image не проверяет CRC; проверяем целостность контейнера до декодирования.
bool completePng(const std::vector<std::uint8_t>& bytes)
{
    static const auto CrcTable = [] {
        std::array<std::uint32_t, 256> table{};
        for (std::uint32_t index = 0; index < 256; ++index) {
            auto value = index;
            for (int bit = 0; bit < 8; ++bit) {
                value = (value >> 1) ^ ((value & 1) ? 0xedb88320U : 0U);
            }
            table[index] = value;
        }
        return table;
    }();
    std::size_t offset = 8;
    bool data = false;
    while (offset <= bytes.size() && bytes.size() - offset >= 12) {
        const auto length = static_cast<std::size_t>(bigEndian(bytes.data() + offset));
        if (length > bytes.size() - offset - 12) {
            return false;
        }
        const auto type = bigEndian(bytes.data() + offset + 4);
        if (offset == 8 && (type != 0x49484452U || length != 13)) {
            return false;
        }
        std::uint32_t crc = 0xffffffffU;
        for (std::size_t index = offset + 4; index < offset + 8 + length; ++index) {
            crc = CrcTable[(crc ^ bytes[index]) & 255] ^ (crc >> 8);
        }
        if ((crc ^ 0xffffffffU) != bigEndian(bytes.data() + offset + 8 + length)) {
            return false;
        }
        offset += length + 12;
        if (type == 0x49444154U) {
            data = true;
        }
        if (type == 0x49454e44U) {
            return data && length == 0 && offset == bytes.size();
        }
    }
    return false;
}

bool imageSize(int width, int height, std::size_t& bytes, ResourceError& error, const ResourceLimits& limits)
{
    if (width <= 0 || height <= 0) {
        error = {ResourceErrorCode::InvalidImage, "Image dimensions must be positive."};
        return false;
    }
    const auto imageWidth = static_cast<std::size_t>(width);
    const auto imageHeight = static_cast<std::size_t>(height);
    if (width > limits.imageDimension || height > limits.imageDimension
        || imageWidth > limits.imageBytes / 4 / imageHeight) {
        error = {ResourceErrorCode::LimitExceeded, "Image dimensions or RGBA byte count exceed the limit."};
        return false;
    }
    bytes = imageWidth * imageHeight * 4;
    return true;
}
}

bool validateImage(const Image& image, ResourceError& error, const ResourceLimits& limits)
{
    std::size_t bytes = 0;
    if (!imageSize(image.width, image.height, bytes, error, limits)) {
        return false;
    }
    if (image.pixels.size() != bytes) {
        error = {ResourceErrorCode::InvalidImage, "RGBA buffer size does not match the image dimensions."};
        return false;
    }
    error = {};
    return true;
}

bool decodePng(const std::vector<std::uint8_t>& bytes, Image& result,
    ResourceError& error, const ResourceLimits& limits)
{
    constexpr std::array<std::uint8_t, 8> Signature = {137, 80, 78, 71, 13, 10, 26, 10};
    if (bytes.size() > limits.fileBytes || bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        error = {ResourceErrorCode::LimitExceeded, "PNG exceeds the encoded size limit."};
        return false;
    }
    if (bytes.size() < Signature.size() || !std::equal(Signature.begin(), Signature.end(), bytes.begin())) {
        error = {ResourceErrorCode::InvalidImage, "Expected a PNG image."};
        return false;
    }
    if (!completePng(bytes)) {
        error = {ResourceErrorCode::InvalidImage, "PNG is truncated or has an invalid checksum."};
        return false;
    }
    Image decoded;
    int channels = 0;
    const int size = static_cast<int>(bytes.size());
    if (!stbi_info_from_memory(bytes.data(), size, &decoded.width, &decoded.height, &channels)) {
        error = {ResourceErrorCode::InvalidImage, "Cannot read PNG dimensions."};
        return false;
    }
    std::size_t pixelBytes = 0;
    if (!imageSize(decoded.width, decoded.height, pixelBytes, error, limits)) {
        return false;
    }
    ImageDecoderMemory::Budget budget(limits.decoderBytes);
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(bytes.data(), size, &decoded.width, &decoded.height, &channels, 4), stbi_image_free);
    if (!pixels) {
        if (budget.exceeded()) {
            error = {ResourceErrorCode::LimitExceeded, "PNG decoder memory limit exceeded."};
            return false;
        }
        const char* reason = stbi_failure_reason();
        error = {ResourceErrorCode::InvalidImage, std::string("Cannot decode PNG: ") + (reason ? reason : "unknown error")};
        return false;
    }
    if (!imageSize(decoded.width, decoded.height, pixelBytes, error, limits)) {
        return false;
    }
    try {
        decoded.pixels.assign(pixels.get(), pixels.get() + pixelBytes);
    } catch (const std::bad_alloc&) {
        error = {ResourceErrorCode::LimitExceeded, "Not enough memory for decoded PNG pixels."};
        return false;
    }
    result = std::move(decoded);
    error = {};
    return true;
}
}
