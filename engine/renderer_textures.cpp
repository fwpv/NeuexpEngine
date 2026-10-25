#include "renderer.h"
#include "detail/texture_data.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <utility>

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

bool valid(const TextureSampling& sampling)
{
    const auto wrap = [](TextureWrap value) { return value == TextureWrap::Clamp || value == TextureWrap::Repeat; };
    return (sampling.filter == TextureFilter::Nearest || sampling.filter == TextureFilter::Linear)
        && wrap(sampling.wrapX) && wrap(sampling.wrapY)
        && std::isfinite(sampling.repeats.x) && sampling.repeats.x > 0
        && std::isfinite(sampling.repeats.y) && sampling.repeats.y > 0;
}
}

bool Renderer2D::setTextureBudget(std::size_t bytes, ResourceError& error)
{
    if (initialized) {
        error = { ResourceErrorCode::InvalidState, "Set the texture budget before renderer initialization." };
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
        error = { ResourceErrorCode::InvalidState, "Initialize the renderer before creating textures." };
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
        error = { ResourceErrorCode::LimitExceeded, "Texture exceeds the GPU dimensions or memory budget." };
        return false;
    }
    try {
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
        std::vector<std::uint8_t> pixels(bytes);
        for (int y = 0; y < height; ++y) {
            const int sourceY = static_cast<int>((static_cast<std::int64_t>(2 * y + 1)
                * image.height) / (2 * height));
            for (int x = 0; x < width; ++x) {
                const int sourceX = static_cast<int>((static_cast<std::int64_t>(2 * x + 1)
                    * image.width) / (2 * width));
                const auto source = (static_cast<std::size_t>(sourceY) * static_cast<std::size_t>(image.width)
                    + static_cast<std::size_t>(sourceX)) * 4;
                const auto target = (static_cast<std::size_t>(y) * storageWidth + static_cast<std::size_t>(x)) * 4;
                std::copy_n(image.pixels.data() + source, 4, pixels.data() + target);
            }
        }
        // Резервирование завершается до создания GPU-объекта.
        textures.reserve(textures.size() + 1);
        GLint previousTexture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
        glPushAttrib(GL_PIXEL_MODE_BIT);
        glPixelTransferi(GL_MAP_COLOR, GL_FALSE);
        for (GLenum parameter : std::array<GLenum, 4>{ GL_RED_SCALE, GL_GREEN_SCALE, GL_BLUE_SCALE, GL_ALPHA_SCALE }) {
            glPixelTransferf(parameter, 1);
        }
        for (GLenum parameter : std::array<GLenum, 4>{ GL_RED_BIAS, GL_GREEN_BIAS, GL_BLUE_BIAS, GL_ALPHA_BIAS }) {
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        const GLenum failure = glGetError();
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
        glPopClientAttrib();
        glPopAttrib();
        if (failure != GL_NO_ERROR || texture->id == 0) {
            glDeleteTextures(1, &texture->id);
            error = { ResourceErrorCode::Graphics, "Cannot upload texture (OpenGL error " + std::to_string(failure) + ")." };
            return false;
        }
        textures.push_back(texture);
        result.data = std::move(texture);
    } catch (const std::bad_alloc&) {
        error = { ResourceErrorCode::LimitExceeded, "Not enough memory to prepare texture pixels." };
        return false;
    }
    error = {};
    return true;
}

bool Renderer2D::loadTexture(const ResourceFiles& files, const std::string& name, Texture& result,
    ResourceError& error, bool keepPixels)
{
    if (!initialized) {
        error = { ResourceErrorCode::InvalidState, "Initialize the renderer before loading textures." };
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
                error = { ResourceErrorCode::InvalidState, "CPU pixels were not retained on the first load: " + name };
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

bool Renderer2D::drawTexture(const Texture& texture, const Rect& rectangle, const DrawOptions& options,
    RenderError& error, const TextureSampling& sampling)
{
    if (!prepareDraw(options, error)) {
        return false;
    }
    if (!texture.isValid() || texture.data->owner != this) {
        error = RenderError::InvalidTexture;
        return false;
    }
    if (!valid(sampling)) {
        error = RenderError::InvalidOptions;
        return false;
    }
    const auto previous = commands.size();
    if (!drawRectangle(rectangle, ShapeFill::Filled, options, error)) {
        return false;
    }
    if (commands.size() > previous) {
        commands.back().texture = texture;
        commands.back().sampling = sampling;
    }
    return true;
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampling.wrapX == TextureWrap::Repeat ? GL_REPEAT : GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampling.wrapY == TextureWrap::Repeat ? GL_REPEAT : GL_CLAMP);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glColor4f(color.red, color.green, color.blue, color.alpha);
    // GL_CLAMP в 1.1 смешивает край с цветом рамки. Разбиение у центров крайних
    // текселей сохраняет внутреннюю интерполяцию и повторяет край без расширений.
    struct Axis
    {
        std::array<float, 4> positions = { 0, 1, 1, 1 };
        int count = 2;
        float halfPixel = 0;
        float repeats = 1;
        bool clamp = false;

        float coordinate(float position) const
        {
            const float value = position * repeats;
            return clamp ? std::clamp(value, halfPixel, 1 - halfPixel) : value;
        }
    };
    const auto axis = [](int size, float repeats, TextureWrap wrap) {
        Axis result;
        result.halfPixel = 0.5F / static_cast<float>(size);
        result.repeats = repeats;
        result.clamp = wrap == TextureWrap::Clamp;
        if (result.clamp) {
            result.positions = { 0, std::min(result.halfPixel / repeats, 1.0F),
                std::min((1 - result.halfPixel) / repeats, 1.0F), 1 };
            result.count = 4;
        }
        return result;
    };
    const Axis horizontal = axis(texture.storageWidth, sampling.repeats.x, sampling.wrapX);
    const Axis vertical = axis(texture.storageHeight, sampling.repeats.y, sampling.wrapY);
    const Vector2 origin = command.vertices[0];
    const Vector2 extent = command.vertices[2] - origin;
    glBegin(GL_QUADS);
    for (int y = 1; y < vertical.count; ++y) {
        for (int x = 1; x < horizontal.count; ++x) {
            const float left = horizontal.positions[static_cast<std::size_t>(x - 1)];
            const float right = horizontal.positions[static_cast<std::size_t>(x)];
            const float top = vertical.positions[static_cast<std::size_t>(y - 1)];
            const float bottom = vertical.positions[static_cast<std::size_t>(y)];
            const std::array<Vector2, 4> positions = { Vector2{left, top}, {right, top}, {right, bottom}, {left, bottom} };
            for (const auto position : positions) {
                glTexCoord2f(horizontal.coordinate(position.x), vertical.coordinate(position.y));
                const auto point = command.options.transform.transformPoint(origin + Vector2{extent.x * position.x, extent.y * position.y});
                glVertex2f(point.x, point.y);
            }
        }
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

}
