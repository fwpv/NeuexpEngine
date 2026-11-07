#include "renderer.h"
#include "detail/texture_data.h"
#include "detail/font_data.h"
#include "geometry.h"
#include "math2d.h"
#include "resources.h"
#include "sprite.h"
#include "texture.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace Engine
{
namespace
{
int powerOfTwo(int value)
{
    int result = 1;
    while (result < value) {
        result *= 2;
    }
    return result;
}

// Ограничивает объём геометрии при программном повторении дополненных текстур.
constexpr double MaximumTextureQuads = 65536;

struct TextureAxisSegment
{
    float begin;
    float end;
    float textureBegin;
    float textureEnd;
};

class TextureAxis
{
public:
    TextureAxis(int imageSize, int storageSize, float start, float span,
        float repeats, TextureWrap wrap, TextureFilter filter);
    std::size_t segmentCount() const { return count; }
    TextureAxisSegment segment(std::size_t index) const;

private:
    std::array<TextureAxisSegment, 3> segments{};
    std::size_t count = 1;
    bool tiled = false;
    float storage;
    float start;
    float span;
    float repeats;
};

TextureAxis::TextureAxis(int imageSize, int storageSize, float start, float span,
    float repeats, TextureWrap wrap, TextureFilter filter)
    : storage(static_cast<float>(storageSize))
    , start(start)
    , span(span)
    , repeats(repeats)
{
    // Целочисленной области Nearest не нужна компенсация линейной фильтрации.
    const bool nearestRegion = filter == TextureFilter::Nearest && repeats == 1
        && start == std::floor(start) && span == std::floor(span) && span > 0;
    if (nearestRegion || (wrap == TextureWrap::Repeat && imageSize == storageSize)) {
        segments[0] = {0, 1, start / storage, start / storage + (span / storage) * repeats};
    } else if (wrap == TextureWrap::Clamp) {
        // GL_CLAMP в 1.1 смешивает край с рамкой; ограничиваем UV центрами крайних текселей.
        const float halfPixel = std::min(0.5F, span / 2);
        const float minimum = start + halfPixel;
        const float maximum = start + span - halfPixel;
        const std::array<float, 4> positions{0,
            std::min(halfPixel / span / repeats, 1.0F),
            std::min((span - halfPixel) / span / repeats, 1.0F), 1};
        count = 3;
        for (std::size_t index = 0; index < count; ++index) {
            const float begin = positions[index];
            const float end = positions[index + 1];
            segments[index] = {begin, end,
                std::clamp(start + begin * repeats * span, minimum, maximum) / storage,
                std::clamp(start + end * repeats * span, minimum, maximum) / storage};
        }
    } else {
        tiled = true;
        const double tiles = std::ceil(static_cast<double>(repeats));
        count = static_cast<std::size_t>(2 * tiles);
        if (repeats - (tiles - 1) <= 0.5 / span) {
            --count;
        }
    }
}

TextureAxisSegment TextureAxis::segment(std::size_t index) const
{
    if (!tiled) {
        return segments[index];
    }
    const double tile = static_cast<double>(index / 2);
    const double end = std::min(1.0, static_cast<double>(repeats) - tile);
    const double seam = 0.5 / span;
    if (index % 2 == 0) {
        // Первый полутексель читается у копии первого столбца/строки за краем изображения.
        // Так Linear смешивает последний тексель с первым, не затрагивая пустое дополнение.
        const double seamEnd = std::min(seam, end);
        return {static_cast<float>(tile / repeats), static_cast<float>((tile + seamEnd) / repeats),
            (start + span) / storage, (start + span + static_cast<float>(seamEnd) * span) / storage};
    }
    return {static_cast<float>((tile + seam) / repeats), static_cast<float>((tile + end) / repeats),
        (start + 0.5F) / storage, (start + static_cast<float>(end) * span) / storage};
}

bool valid(const TextureSampling& sampling)
{
    const auto wrap = [](TextureWrap value) { return value == TextureWrap::Clamp || value == TextureWrap::Repeat; };
    return (sampling.filter == TextureFilter::Nearest || sampling.filter == TextureFilter::Linear)
        && wrap(sampling.wrapX) && wrap(sampling.wrapY)
        && std::isfinite(sampling.repeats.x) && sampling.repeats.x > 0
        && std::isfinite(sampling.repeats.y) && sampling.repeats.y > 0;
}
}

void Detail::prepareTextGeometry(TextData& text, const TextureData& texture)
{
    for (std::size_t mode = 0; mode < text.vertices.size(); ++mode) {
        const auto filter = mode == 0 ? TextureFilter::Nearest : TextureFilter::Linear;
        auto& vertices = text.vertices[mode];
        vertices.reserve(text.glyphs.size() * (mode == 0 ? 4 : 36));
        for (const auto& glyph : text.glyphs) {
            const TextureAxis horizontal(texture.width, texture.storageWidth, glyph.area.position.x,
                glyph.area.size.width, 1, TextureWrap::Clamp, filter);
            const TextureAxis vertical(texture.height, texture.storageHeight, glyph.area.position.y,
                glyph.area.size.height, 1, TextureWrap::Clamp, filter);
            for (std::size_t row = 0; row < vertical.segmentCount(); ++row) {
                const auto y = vertical.segment(row);
                for (std::size_t column = 0; column < horizontal.segmentCount(); ++column) {
                    const auto x = horizontal.segment(column);
                    const auto positions = Rect{x.begin, y.begin, x.end - x.begin, y.end - y.begin}.corners();
                    const auto coordinates = Rect{x.textureBegin, y.textureBegin,
                        x.textureEnd - x.textureBegin, y.textureEnd - y.textureBegin}.corners();
                    for (std::size_t index = 0; index < positions.size(); ++index) {
                        vertices.push_back(TextVertex{glyph.bounds.position + Vector2{
                            glyph.bounds.size.width * positions[index].x, glyph.bounds.size.height * positions[index].y},
                            coordinates[index]});
                    }
                }
            }
        }
    }
}

bool Renderer2D::setTextureBudget(std::size_t bytes, ResourceError& error)
{
    if (initialized) {
        error = {ResourceErrorCode::InvalidState, "Set the texture budget before renderer initialization."};
        return false;
    }
    textureBudget = bytes;
    error = {};
    return true;
}

TextureStats Renderer2D::textureStats() const
{
    TextureStats result;
    result.count = textures.size();
    for (const auto& texture : textures) {
        result.gpuBytes += texture->gpuBytes;
        if (texture->image) {
            result.retainedPixelBytes += texture->image->pixels.size();
        }
    }
    return result;
}

void Renderer2D::collectUnusedTextures()
{
    auto iterator = textures.begin();
    while (iterator != textures.end()) {
        if (iterator->use_count() == 1) {
            glDeleteTextures(1, &(*iterator)->id);
            iterator = textures.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

bool Renderer2D::createTexture(const Image& image, Texture& result, ResourceError& error, bool keepPixels)
{
    if (!initialized) {
        error = {ResourceErrorCode::InvalidState, "Initialize the renderer before creating textures."};
        return false;
    }
    if (!validateImage(image, error)) {
        return false;
    }
    const int width = powerOfTwo(image.width);
    const int height = powerOfTwo(image.height);
    GLint maximum = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    const auto storageWidth = static_cast<std::size_t>(width);
    const auto storageHeight = static_cast<std::size_t>(height);
    const auto bytes = storageWidth * storageHeight * 4;
    collectUnusedTextures();
    const auto used = textureStats().gpuBytes;
    if (width > maximum || height > maximum || bytes > textureBudget || used > textureBudget - bytes) {
        error = {ResourceErrorCode::LimitExceeded, "Texture exceeds the GPU dimensions or memory budget."};
        return false;
    }
    auto texture = std::make_shared<Detail::TextureData>();
    texture->owner = this;
    texture->width = image.width;
    texture->height = image.height;
    texture->storageWidth = width;
    texture->storageHeight = height;
    texture->gpuBytes = bytes;
    if (keepPixels) {
        texture->image = std::make_unique<Image>(image);
    }
    std::vector<std::uint8_t> pixels;
    const std::uint8_t* uploadPixels = image.pixels.data();
    if (width != image.width || height != image.height) {
        pixels.resize(bytes);
        const auto rowBytes = static_cast<std::size_t>(image.width) * 4;
        const auto storageRowBytes = storageWidth * 4;
        // За правым и нижним краем — копия противоположного края для Repeat;
        // остальное дополнение остаётся прозрачным чёрным.
        for (int y = 0; y < image.height; ++y) {
            const auto* source = image.pixels.data() + static_cast<std::size_t>(y) * rowBytes;
            auto* target = pixels.data() + static_cast<std::size_t>(y) * storageRowBytes;
            std::copy_n(source, rowBytes, target);
            if (width > image.width) {
                std::copy_n(source, 4, target + rowBytes);
            }
        }
        if (height > image.height) {
            std::copy_n(pixels.data(), storageRowBytes,
                pixels.data() + static_cast<std::size_t>(image.height) * storageRowBytes);
        }
        uploadPixels = pixels.data();
    }
    // Добавляем до создания GPU-объекта, чтобы перераспределение вектора не могло вызвать его утечку.
    textures.push_back(texture);
    GLint previousTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glPushAttrib(GL_PIXEL_MODE_BIT);
    glPixelTransferi(GL_MAP_COLOR, GL_FALSE);
    for (GLenum parameter : std::array<GLenum, 4>{GL_RED_SCALE, GL_GREEN_SCALE, GL_BLUE_SCALE, GL_ALPHA_SCALE}) {
        glPixelTransferf(parameter, 1);
    }
    for (GLenum parameter : std::array<GLenum, 4>{GL_RED_BIAS, GL_GREEN_BIAS, GL_BLUE_BIAS, GL_ALPHA_BIAS}) {
        glPixelTransferf(parameter, 0);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glGenTextures(1, &texture->id);
    glBindTexture(GL_TEXTURE_2D, texture->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, uploadPixels);
    const GLenum failure = glGetError();
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    glPopClientAttrib();
    glPopAttrib();
    if (failure != GL_NO_ERROR || texture->id == 0) {
        glDeleteTextures(1, &texture->id);
        textures.pop_back();
        error = {ResourceErrorCode::Graphics, "Cannot upload texture (OpenGL error " + std::to_string(failure) + ")."};
        return false;
    }
    result.data = std::move(texture);
    error = {};
    return true;
}

bool Renderer2D::loadTexture(const ResourceFiles& files, const std::string& name, Texture& result,
    ResourceError& error, bool keepPixels)
{
    if (!initialized) {
        error = {ResourceErrorCode::InvalidState, "Initialize the renderer before loading textures."};
        return false;
    }
    std::filesystem::path path;
    if (!files.resolve(name, path, error)) {
        return false;
    }
    for (const auto& texture : textures) {
        std::error_code pathError;
        if (texture->path == path || (!texture->path.empty()
            && std::filesystem::equivalent(texture->path, path, pathError) && !pathError)) {
            if (keepPixels && !texture->image) {
                error = {ResourceErrorCode::InvalidState, "CPU pixels were not retained on the first load: " + name};
                return false;
            }
            result.data = texture;
            error = {};
            return true;
        }
    }
    Image image;
    if (!files.loadPng(name, image, error) || !createTexture(image, result, error, keepPixels)) {
        return false;
    }
    result.data->path = std::move(path);
    return true;
}

void Renderer2D::drawTexture(const Texture& texture, const Rect& rectangle, const DrawOptions& options,
    const TextureSampling& sampling)
{
    queueTexture(texture, rectangle, options, sampling, std::nullopt);
}

void Renderer2D::drawSprite(const Sprite& sprite, const DrawOptions& options)
{
    if (!ready() || !sprite.visible) {
        return;
    }
    DrawOptions combined;
    combined.transform = sprite.transform.toAffine();
    combined.color = sprite.color;
    if (!prepareDraw(options) || !prepareDraw(combined)) {
        return;
    }
    combined = options;
    combined.transform = options.transform * sprite.transform.toAffine();
    combined.color = {options.color.red * sprite.color.red, options.color.green * sprite.color.green,
        options.color.blue * sprite.color.blue, options.color.alpha * sprite.color.alpha};
    TextureSampling sampling;
    sampling.filter = sprite.filter;
    queueTexture(sprite.texture, Rect{Vector2{}, sprite.size}, combined, sampling, sprite.textureRect);
}

void Renderer2D::queueTexture(const Texture& texture, const Rect& rectangle, const DrawOptions& options,
    const TextureSampling& sampling, const std::optional<Rect>& textureRect)
{
    if (!ready()) {
        return;
    }
    if (!prepareDraw(options)) {
        return;
    }
    if (!texture.isValid() || texture.data->owner != this) {
        firstError = RenderError::InvalidTexture;
        return;
    }
    if (!valid(sampling)) {
        firstError = RenderError::InvalidOptions;
        return;
    }
    const auto segmentCount = [](int size, int storage, float repeats, TextureWrap wrap) {
        return wrap == TextureWrap::Repeat ? (size == storage ? 1.0 : 2 * std::ceil(static_cast<double>(repeats))) : 3.0;
    };
    if (segmentCount(texture.width(), texture.data->storageWidth, sampling.repeats.x, sampling.wrapX)
        * segmentCount(texture.height(), texture.data->storageHeight, sampling.repeats.y, sampling.wrapY) > MaximumTextureQuads) {
        firstError = RenderError::InvalidOptions;
        return;
    }
    if (textureRect) {
        const auto& area = *textureRect;
        if (!std::isfinite(area.position.x) || !std::isfinite(area.position.y)
            || !std::isfinite(area.size.width) || !std::isfinite(area.size.height)
            || area.position.x < 0 || area.position.y < 0 || area.size.width < 0 || area.size.height < 0
            || static_cast<double>(area.position.x) + area.size.width > texture.width()
            || static_cast<double>(area.position.y) + area.size.height > texture.height()) {
            firstError = RenderError::InvalidGeometry;
            return;
        }
    }
    const auto previous = commands.size();
    if (!queueRectangle(rectangle, ShapeFill::Filled, options)) {
        return;
    }
    if (commands.size() > previous) {
        commands.back().texture = texture;
        commands.back().sampling = sampling;
        commands.back().textureRect = textureRect;
    }
}

void Renderer2D::executeTexture(const Command& command) const
{
    const auto& texture = *command.texture.data;
    const auto& sampling = command.sampling;
    const auto& color = command.options.color;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    const GLint filter = sampling.filter == TextureFilter::Nearest ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampling.wrapX == TextureWrap::Repeat && texture.width == texture.storageWidth ? GL_REPEAT : GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampling.wrapY == TextureWrap::Repeat && texture.height == texture.storageHeight ? GL_REPEAT : GL_CLAMP);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glColor4f(color.red, color.green, color.blue, color.alpha);
    const Rect area = command.textureRect.value_or(Rect{0, 0,
        static_cast<float>(texture.width), static_cast<float>(texture.height)});
    const TextureAxis horizontal(texture.width, texture.storageWidth, area.position.x,
        area.size.width, sampling.repeats.x, sampling.wrapX, sampling.filter);
    const TextureAxis vertical(texture.height, texture.storageHeight, area.position.y,
        area.size.height, sampling.repeats.y, sampling.wrapY, sampling.filter);
    const Vector2 origin = command.vertices[0];
    const Vector2 extent = command.vertices[2] - origin;
    glBegin(GL_QUADS);
    for (std::size_t row = 0; row < vertical.segmentCount(); ++row) {
        const auto y = vertical.segment(row);
        for (std::size_t column = 0; column < horizontal.segmentCount(); ++column) {
            const auto x = horizontal.segment(column);
            const Quad positions = Rect{x.begin, y.begin, x.end - x.begin, y.end - y.begin}.corners();
            const Quad coordinates = Rect{x.textureBegin, y.textureBegin,
                x.textureEnd - x.textureBegin, y.textureEnd - y.textureBegin}.corners();
            for (std::size_t index = 0; index < positions.size(); ++index) {
                glTexCoord2f(coordinates[index].x, coordinates[index].y);
                const auto point = command.options.transform.transformPoint(origin
                    + Vector2{extent.x * positions[index].x, extent.y * positions[index].y});
                glVertex2f(point.x, point.y);
            }
        }
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

}
