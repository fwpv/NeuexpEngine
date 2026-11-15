#include "game.h"
#include "atlas.h"
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
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace Cat
{
namespace
{
bool loadAnimation(NeuexpEngine::Renderer2D& renderer, const NeuexpEngine::ResourceFiles& files,
    const std::string& name, std::size_t frameCount,
    NeuexpEngine::Animation& animation, NeuexpEngine::ResourceError& error)
{
    NeuexpEngine::Image image;
    if (!files.loadPng("cat/" + name + "_atlas.png", image, error)) {
        return false;
    }
    // Файлы атласов сохраняют исходные пиксели; чёрный становится прозрачным только в игре.
    for (std::size_t pixel = 0; pixel < image.pixels.size(); pixel += 4) {
        image.pixels[pixel + 3] = image.pixels[pixel] == 0 && image.pixels[pixel + 1] == 0
            && image.pixels[pixel + 2] == 0 ? 0 : 255;
    }
    NeuexpEngine::Texture texture;
    NeuexpEngine::TextureAtlas atlas;
    if (!renderer.createTexture(image, texture, error)
        || !atlas.load(texture, files, "cat/" + name + ".atlas", error)) {
        return false;
    }
    std::vector<NeuexpEngine::Rect> frames;
    for (std::size_t index = 0; index < frameCount; ++index) {
        const auto* area = atlas.find(std::to_string(index));
        if (!area) {
            error = NeuexpEngine::ResourceError{NeuexpEngine::ResourceErrorCode::InvalidImage,
                "Missing animation frame: " + name + "/" + std::to_string(index)};
            return false;
        }
        frames.push_back(*area);
    }
    return animation.load(texture, frames, 0.03F, error);
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
    const float previousX = world.catPosition().x;
    world.update(duration, controls, visibleWidth);
    distance += std::abs(static_cast<double>(world.catPosition().x) - previousX);
    foliage.update(duration, world.catPosition().x - 101.25F);
}
bool Game::isOver() const
{
    // Опора кота ниже сцены на высоту его изображения: кот полностью скрылся.
    constexpr float FallBoundary = 700 + 123.75F;
    return world.catPosition().y >= FallBoundary;
}

bool Game::prepareView(int width, int height, NeuexpEngine::CameraError& error)
{
    const float scale = std::min(static_cast<float>(width) / 935, static_cast<float>(height) / 700);
    const int viewHeight = static_cast<int>(700.0F * scale);
    NeuexpEngine::Camera2 camera;
    camera.referenceSize = NeuexpEngine::Size2{static_cast<float>(width) / scale, static_cast<float>(viewHeight) / scale};
    camera.position = NeuexpEngine::Vector2{world.catPosition().x + camera.referenceSize.width / 2 - 450, camera.referenceSize.height / 2};
    if (!camera.createView(NeuexpEngine::Viewport{0, (height - viewHeight) / 2, width, viewHeight}, view, error)) {
        return false;
    }
    visibleWidth = view.worldBounds.size.width;
    return true;
}

void Game::draw(NeuexpEngine::Renderer2D& renderer, const Images& images) const
{
    renderer.setWorldView(view);
    foliage.draw(renderer, images, view.worldBounds);
    world.draw(renderer, images, view.worldBounds);
}

bool Images::load(NeuexpEngine::Renderer2D& renderer, NeuexpEngine::ResourceError& error)
{
    NeuexpEngine::ResourceFiles files;
    if (!files.initialize(error)) {
        return false;
    }
    constexpr std::array<std::string_view, ArtCount> Files{
        "platform.png", "platform2.png", "platform3.png", "grib.png", "flower2.png", "flower1.png",
        "label1.png", "label2.png", "label3.png"
    };
    for (std::size_t index = 0; index < Files.size(); ++index) {
        NeuexpEngine::Image image;
        if (!files.loadPng("cat/" + std::string(Files[index]), image, error)) {
            return false;
        }
        // Чёрный — цветовой ключ статических изображений.
        constexpr std::size_t RgbaChannels = 4;
        for (std::size_t pixel = 0; pixel < image.pixels.size(); pixel += RgbaChannels) {
            image.pixels[pixel + 3] = image.pixels[pixel] == 0 && image.pixels[pixel + 1] == 0
                && image.pixels[pixel + 2] == 0 ? 0 : 255;
        }
        NeuexpEngine::Texture texture;
        if (!renderer.createTexture(image, texture, error)) {
            return false;
        }
        sprites[index] = NeuexpEngine::Sprite(texture);
        sprites[index].filter = NeuexpEngine::TextureFilter::Nearest;
    }
    if (!loadAnimation(renderer, files, "head", 5, head, error)
        || !loadAnimation(renderer, files, "feet", 12, feet, error)) {
        return false;
    }
    // Атлас обрезан на 9 пустых пикселей слева; сохраняем положение относительно лап.
    head.transform.position.x = 9;
    head.setMode(NeuexpEngine::AnimationMode::Clamp);
    head.setFrame(head.frameCount() / 2);
    return true;
}

void Images::draw(NeuexpEngine::Renderer2D& renderer, std::size_t image, NeuexpEngine::Vector2 position, float scale, float angle) const
{
    NeuexpEngine::Sprite sprite = sprites[image];
    sprite.size.width *= scale;
    sprite.size.height *= scale;
    sprite.transform.position = position;
    if (image >= Leaf1 && image <= Leaf3) {
        // Центр задан в единицах сцены и не масштабируется вместе с изображением.
        constexpr float LeafPivot = 17.5F;
        sprite.transform.position += NeuexpEngine::Vector2{LeafPivot, LeafPivot};
        sprite.transform.origin = NeuexpEngine::Vector2{LeafPivot, LeafPivot};
        sprite.transform.rotation = -NeuexpEngine::degreesToRadians(angle);
    }
    renderer.drawSprite(sprite);
}

}
