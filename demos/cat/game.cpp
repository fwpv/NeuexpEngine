#include "game.h"
#include "camera.h"
#include "geometry.h"
#include "math2d.h"
#include "renderer.h"
#include "resources.h"
#include "sprite.h"
#include "texture.h"
#include "transform.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace Cat
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

bool loadAnimation(Engine::Renderer2D& renderer, const std::vector<Engine::Image>& frames,
    Engine::Animation& animation, Engine::ResourceError& error)
{
    const int width = powerOfTwo(frames.front().width);
    const int height = powerOfTwo(frames.front().height);
    const int columns = 2;
    const int rows = (static_cast<int>(frames.size()) + columns - 1) / columns;
    Engine::Image atlas{powerOfTwo(columns * width), powerOfTwo(rows * height), std::vector<std::uint8_t>{}};
    atlas.pixels.resize(static_cast<std::size_t>(atlas.width) * static_cast<std::size_t>(atlas.height) * 4);
    std::vector<Engine::Rect> areas;
    for (std::size_t index = 0; index < frames.size(); ++index) {
        const auto& source = frames[index];
        const int left = static_cast<int>(index) % columns * width;
        const int top = static_cast<int>(index) / columns * height;
        // Масштабирование каждого кадра совпадает с загрузкой отдельной текстуры OpenGL 1.1.
        // Рендерер ограничивает линейную фильтрацию границами каждого участка.
        for (int y = 0; y < height; ++y) {
            const int sourceY = (2 * y + 1) * source.height / (2 * height);
            for (int x = 0; x < width; ++x) {
                const int sourceX = (2 * x + 1) * source.width / (2 * width);
                const auto from = static_cast<std::size_t>(sourceY * source.width + sourceX) * 4;
                const auto to = static_cast<std::size_t>((top + y) * atlas.width + left + x) * 4;
                std::copy_n(source.pixels.data() + from, 4, atlas.pixels.data() + to);
            }
        }
        areas.push_back(Engine::Rect{static_cast<float>(left), static_cast<float>(top),
            static_cast<float>(width), static_cast<float>(height)});
    }
    Engine::Texture texture;
    if (!renderer.createTexture(atlas, texture, error) || !animation.load(texture, areas, 0.03F, error)) {
        return false;
    }
    animation.size = Engine::Size2{static_cast<float>(frames.front().width), static_cast<float>(frames.front().height)};
    return true;
}
}

Game::Game(std::uint32_t routeSeed, std::uint32_t leavesSeed, const Images& images)
    : world(routeSeed, images)
    , foliage(leavesSeed)
{
}

void Game::update(float duration, const Controls& controls)
{
    // Листья используют позицию после движения; маршрут — до движения.
    world.update(duration, controls, visibleWidth);
    foliage.update(duration, world.catPosition().x - 101.25F);
}
bool Game::prepareView(int width, int height, Engine::CameraError& error)
{
    const float scale = std::min(static_cast<float>(width) / 935, static_cast<float>(height) / 700);
    const int viewHeight = static_cast<int>(700.0F * scale);
    Engine::Camera2 camera;
    camera.referenceSize = Engine::Size2{static_cast<float>(width) / scale, static_cast<float>(viewHeight) / scale};
    camera.position = Engine::Vector2{world.catPosition().x + camera.referenceSize.width / 2 - 450, camera.referenceSize.height / 2};
    if (!camera.createView(Engine::Viewport{0, (height - viewHeight) / 2, width, viewHeight}, view, error)) {
        return false;
    }
    visibleWidth = view.worldBounds.size.width;
    return true;
}

void Game::draw(Engine::Renderer2D& renderer, const Images& images) const
{
    renderer.setWorldView(view);
    foliage.draw(renderer, images, view.worldBounds);
    world.draw(renderer, images, view.worldBounds);
}

bool Images::load(Engine::Renderer2D& renderer, Engine::ResourceError& error)
{
    Engine::ResourceFiles files;
    if (!files.initialize(error)) {
        return false;
    }
    constexpr std::size_t FeetImage = ArtCount + 5;
    constexpr std::array<std::string_view, FeetImage + 1> Files{
        "platform.png", "platform2.png", "platform3.png", "grib.png", "flower2.png", "flower1.png",
        "label1.png", "label2.png", "label3.png", "head5.png", "head4.png", "head1.png", "head2.png", "head3.png", "feet.png"
    };
    std::vector<Engine::Image> headFrames;
    std::vector<Engine::Image> feetFrames;
    for (std::size_t index = 0; index < Files.size(); ++index) {
        Engine::Image source;
        if (!files.loadPng("cat/" + std::string(Files[index]), source, error)) {
            return false;
        }
        // Чёрный — цветовой ключ; альфа-канал исходных PNG не используется.
        constexpr std::size_t RgbaChannels = 4;
        for (std::size_t pixel = 0; pixel < source.pixels.size(); pixel += RgbaChannels) {
            source.pixels[pixel + 3] = source.pixels[pixel] == 0 && source.pixels[pixel + 1] == 0
                && source.pixels[pixel + 2] == 0 ? 0 : 255;
        }
        const bool leaf = index >= Leaf1 && index <= Leaf3;
        constexpr int LeafTextureSize = 64;
        constexpr int FeetWidth = 180;
        constexpr int FeetHeight = 53;
        constexpr int FeetColumns = 2;
        constexpr int FeetFrames = 12;
        if ((index >= ArtCount && index < FeetImage && (source.width != 270 || source.height != 160))
            || (leaf && (source.width > LeafTextureSize || source.height > LeafTextureSize))
            || (index == FeetImage && (source.width < FeetColumns * FeetWidth
                || source.height < FeetFrames / FeetColumns * FeetHeight))) {
            error = Engine::ResourceError{Engine::ResourceErrorCode::InvalidImage, "Unexpected dimensions: " + std::string(Files[index])};
            return false;
        }
        const int frames = index == FeetImage ? FeetFrames : 1;
        for (int frame = 0; frame < frames; ++frame) {
            const int width = index == FeetImage ? FeetWidth : source.width;
            const int height = index == FeetImage ? FeetHeight : source.height;
            const int left = index == FeetImage ? frame % FeetColumns * width : 0;
            const int top = index == FeetImage ? frame / FeetColumns * height : 0;
            // Листья дополнены до 64x64: OpenGL 1.1 не должен растягивать их тексели.
            Engine::Image image{leaf ? LeafTextureSize : width, leaf ? LeafTextureSize : height, std::vector<std::uint8_t>{}};
            image.pixels.resize(static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * RgbaChannels);
            for (int row = 0; row < height; ++row) {
                const auto sourceOffset = static_cast<std::size_t>((top + row) * source.width + left) * RgbaChannels;
                const auto destinationOffset = static_cast<std::size_t>(row * image.width) * RgbaChannels;
                std::copy_n(source.pixels.data() + sourceOffset,
                    static_cast<std::size_t>(width) * RgbaChannels, image.pixels.data() + destinationOffset);
            }
            if (index >= ArtCount) {
                (index == FeetImage ? feetFrames : headFrames).push_back(std::move(image));
                continue;
            }
            Engine::Texture texture;
            if (!renderer.createTexture(image, texture, error)) {
                return false;
            }
            auto& sprite = sprites[index + static_cast<std::size_t>(frame)];
            sprite = Engine::Sprite(texture);
            sprite.filter = Engine::TextureFilter::Nearest;
        }
    }
    if (!loadAnimation(renderer, headFrames, head, error)
        || !loadAnimation(renderer, feetFrames, feet, error)) {
        return false;
    }
    head.setMode(Engine::AnimationMode::Clamp);
    head.setFrame(head.frameCount() / 2);
    return true;
}

void Images::draw(Engine::Renderer2D& renderer, std::size_t image, Engine::Vector2 position, float scale, float angle) const
{
    Engine::Sprite sprite = sprites[image];
    sprite.size.width *= scale;
    sprite.size.height *= scale;
    sprite.transform.position = position;
    if (image >= Leaf1 && image <= Leaf3) {
        // Центр задан в единицах сцены и не масштабируется вместе с изображением.
        constexpr float LeafPivot = 17.5F;
        sprite.transform.position += Engine::Vector2{LeafPivot, LeafPivot};
        sprite.transform.origin = Engine::Vector2{LeafPivot, LeafPivot};
        sprite.transform.rotation = -Engine::degreesToRadians(angle);
    }
    renderer.drawSprite(sprite);
}

}
