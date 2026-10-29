#include "game.h"
#include "math2d.h"
#include "transform.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Cat
{
bool Images::load(Engine::Renderer2D& renderer, Engine::ResourceError& error)
{
    constexpr int FeetFrames = ArtCount - Feet;
    Engine::ResourceFiles files;
    if (!files.initialize(error)) {
        return false;
    }
    constexpr std::array<std::string_view, Feet + 1> Files{
        "platform.png", "platform2.png", "platform3.png", "grib.png", "flower2.png", "flower1.png",
        "label1.png", "label2.png", "label3.png", "head5.png", "head4.png", "head1.png", "head2.png", "head3.png", "feet.png"
    };
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
        if ((leaf && (source.width > LeafTextureSize || source.height > LeafTextureSize))
            || (index == Feet && (source.width < FeetColumns * FeetWidth
                || source.height < FeetFrames / FeetColumns * FeetHeight))) {
            error = Engine::ResourceError{Engine::ResourceErrorCode::InvalidImage, "Unexpected dimensions: " + std::string(Files[index])};
            return false;
        }
        const int frames = index == Feet ? FeetFrames : 1;
        for (int frame = 0; frame < frames; ++frame) {
            const int width = index == Feet ? FeetWidth : source.width;
            const int height = index == Feet ? FeetHeight : source.height;
            const int left = index == Feet ? frame % FeetColumns * width : 0;
            const int top = index == Feet ? frame / FeetColumns * height : 0;
            // Листья дополнены до 64x64: OpenGL 1.1 не должен растягивать их тексели.
            Engine::Image image{leaf ? LeafTextureSize : width, leaf ? LeafTextureSize : height, std::vector<std::uint8_t>{}};
            image.pixels.resize(static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * RgbaChannels);
            for (int row = 0; row < height; ++row) {
                const auto sourceOffset = static_cast<std::size_t>((top + row) * source.width + left) * RgbaChannels;
                const auto destinationOffset = static_cast<std::size_t>(row * image.width) * RgbaChannels;
                std::copy_n(source.pixels.data() + sourceOffset,
                    static_cast<std::size_t>(width) * RgbaChannels, image.pixels.data() + destinationOffset);
            }
            if (!renderer.createTexture(image, textures[index + static_cast<std::size_t>(frame)], error)) {
                return false;
            }
        }
    }
    return true;
}

void Images::draw(Engine::Renderer2D& renderer, std::size_t image, Point screen, double scale, double angle) const
{
    const auto& texture = textures[image];
    Engine::Rect rectangle{static_cast<float>(screen.x), static_cast<float>(screen.y),
        static_cast<float>(texture.width() * scale), static_cast<float>(texture.height() * scale)};
    Engine::DrawOptions options;
    if (image >= Leaf1 && image <= Leaf3) {
        constexpr float LeafPivot = 17.5F;
        Engine::Transform2 transform;
        transform.position = Engine::Vector2{rectangle.position.x + LeafPivot, rectangle.position.y + LeafPivot};
        transform.rotation = -Engine::degreesToRadians(static_cast<float>(angle));
        options.transform = transform.toAffine();
        rectangle.position = Engine::Vector2{-LeafPivot, -LeafPivot};
    }
    renderer.drawTexture(texture, rectangle, options,
        Engine::TextureSampling{image >= Head ? Engine::TextureFilter::Linear : Engine::TextureFilter::Nearest});
}

void Platform::draw(Engine::Renderer2D& renderer, const Images& images, double camera, double width) const
{
    constexpr double PlatformCullBehind = 250;
    constexpr double PlatformCullAhead = 5;
    if (position.x - camera <= -PlatformCullBehind || position.x - camera >= width + PlatformCullAhead) {
        return;
    }
    images.draw(renderer, art, Point{position.x - camera, position.y});
    for (const auto& decoration : decorations) {
        images.draw(renderer, decoration.art, Point{position.x + decoration.offset.x - camera, position.y + decoration.offset.y});
    }
}

void Leaf::draw(Engine::Renderer2D& renderer, const Images& images, double camera, double width) const
{
    if (active && position.x >= camera - 25.0 && position.x <= camera + width + 25.0
        && position.y >= -25.0 && position.y <= 700 + 25.0) {
        images.draw(renderer, art, Point{position.x - camera, position.y}, 1, angle);
    }
}

void Cat::draw(Engine::Renderer2D& renderer, const Images& images, double camera) const
{
    const Point origin{feet.x - 101.25 - camera, feet.y - 123.75 + 3.0 + (support >= 0 ? 2.0 : 0)};
    images.draw(renderer, Head + static_cast<std::size_t>(headFrame), origin, 0.75);
    images.draw(renderer, Feet + static_cast<std::size_t>(feetFrame),
        Point{origin.x + 44 * 0.75, origin.y + 120 * 0.75}, 0.75);
}

bool Game::resize(int width, int height, Engine::UiError& error)
{
    const float scale = std::min(static_cast<float>(width) / 935, static_cast<float>(height) / 700);
    const int viewHeight = static_cast<int>(700.0 * scale);
    if (!Engine::UiCanvas{scale}.createView(Engine::Viewport{0, (height - viewHeight) / 2, width, viewHeight}, view, error)) {
        return false;
    }
    visibleWidth = static_cast<double>(width) / scale;
    return true;
}

void Game::draw(Engine::Renderer2D& renderer, const Images& images) const
{
    renderer.setUiView(view);
    const double camera = cat.position().x - 450.0;
    for (const auto& leaf : leaves) {
        leaf.draw(renderer, images, camera, visibleWidth);
    }
    for (const auto& platform : platforms) {
        platform.draw(renderer, images, camera, visibleWidth);
    }
    cat.draw(renderer, images, camera);
}
}
