#include "game.h"
#include "camera.h"
#include "geometry.h"
#include "math2d.h"
#include "renderer.h"
#include "resources.h"
#include "texture.h"
#include "transform.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Cat
{
Game::Game(std::uint32_t routeSeed, std::uint32_t leavesSeed)
    : world(routeSeed)
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
        constexpr int FeetFrames = ArtCount - Feet;
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

void Images::draw(Engine::Renderer2D& renderer, std::size_t image, Engine::Vector2 position, float scale, float angle) const
{
    const auto& texture = textures[image];
    Engine::Rect rectangle{position, Engine::Size2{static_cast<float>(texture.width()) * scale,
        static_cast<float>(texture.height()) * scale}};
    Engine::DrawOptions options;
    if (image >= Leaf1 && image <= Leaf3) {
        constexpr float LeafPivot = 17.5F;
        Engine::Transform2 transform;
        transform.position = Engine::Vector2{rectangle.position.x + LeafPivot, rectangle.position.y + LeafPivot};
        transform.rotation = -Engine::degreesToRadians(angle);
        options.transform = transform.toAffine();
        rectangle.position = Engine::Vector2{-LeafPivot, -LeafPivot};
    }
    renderer.drawTexture(texture, rectangle, options,
        Engine::TextureSampling{image >= Head ? Engine::TextureFilter::Linear : Engine::TextureFilter::Nearest});
}

}
