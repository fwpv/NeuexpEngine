#include "world.h"
#include "core/transform.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace SceneDemo
{
namespace
{
// Клетка текстуры и размер кадра в локальных единицах.
constexpr int Cell = 18;
constexpr int ArtCount = 4;
// Скорость охотника в единицах мира за секунду.
constexpr float HunterSpeed = 90;

NeuexpEngine::Vector2 worldCenter()
{
    return {WorldWidth / 2, WorldHeight / 2};
}

void writePixel(NeuexpEngine::Image& image, int art, int x, int y,
    float red, float green, float blue, float alpha)
{
    const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width)
        + static_cast<std::size_t>(art * Cell + x)) * 4;
    image.pixels[offset] = static_cast<std::uint8_t>(red * 255);
    image.pixels[offset + 1] = static_cast<std::uint8_t>(green * 255);
    image.pixels[offset + 2] = static_cast<std::uint8_t>(blue * 255);
    image.pixels[offset + 3] = static_cast<std::uint8_t>(alpha * 255);
}

// Простые фигуры кадров: камень, корабль, звезда и ящик.
void paintArt(NeuexpEngine::Image& image)
{
    const float middle = (Cell - 1) / 2.0F;
    for (int art = 0; art < ArtCount; ++art) {
        const Art kind = static_cast<Art>(art);
        for (int y = 0; y < Cell; ++y) {
            for (int x = 0; x < Cell; ++x) {
                const float dx = static_cast<float>(x) - middle;
                const float dy = static_cast<float>(y) - middle;
                const float distance = std::sqrt(dx * dx + dy * dy);
                float red = 0;
                float green = 0;
                float blue = 0;
                float alpha = 0;
                switch (kind) {
                case Art::Rock:
                    if (distance <= 7.5F) {
                        red = 0.62F;
                        green = 0.58F;
                        blue = 0.52F;
                        alpha = 1;
                    }
                    if (distance > 6.0F && distance <= 7.5F) {
                        red = 0.38F;
                        green = 0.35F;
                        blue = 0.31F;
                    }
                    if (distance <= 2.5F && dx < 0 && dy < 0) {
                        red = 0.82F;
                        green = 0.78F;
                        blue = 0.72F;
                    }
                    break;
                case Art::Ship:
                    if (x >= 2 && x <= 15 && std::abs(dy) <= static_cast<float>(x - 1) / 2.0F) {
                        red = 0.35F;
                        green = 0.66F;
                        blue = 0.95F;
                        alpha = 1;
                    }
                    if (x >= 5 && x <= 9 && std::abs(dy) <= 1.5F) {
                        red = 0.85F;
                        green = 0.93F;
                        blue = 1;
                    }
                    break;
                case Art::Star:
                    if ((std::abs(dx) <= 1.5F && std::abs(dy) <= 6.5F)
                        || (std::abs(dy) <= 1.5F && std::abs(dx) <= 6.5F)) {
                        red = 1;
                        green = 0.85F;
                        blue = 0.32F;
                        alpha = 1;
                    }
                    break;
                case Art::Box:
                    if (x >= 2 && x <= 15 && y >= 2 && y <= 15) {
                        red = 0.45F;
                        green = 0.75F;
                        blue = 0.47F;
                        alpha = 1;
                        if (x == 2 || x == 15 || y == 2 || y == 15) {
                            red = 0.22F;
                            green = 0.42F;
                            blue = 0.24F;
                        }
                    }
                    break;
                }
                writePixel(image, art, x, y, red, green, blue, alpha);
            }
        }
    }
}
}

NeuexpEngine::Image makeArtImage()
{
    NeuexpEngine::Image image{Cell * ArtCount, Cell,
        std::vector<std::uint8_t>(static_cast<std::size_t>(Cell * ArtCount) * Cell * 4, 0)};
    paintArt(image);
    return image;
}

NeuexpEngine::Sprite makeSprite(const NeuexpEngine::Texture& texture, Art art)
{
    const int cell = static_cast<int>(art);
    NeuexpEngine::Sprite sprite{texture, NeuexpEngine::Rect{
        static_cast<float>(cell * Cell), 0, static_cast<float>(Cell), static_cast<float>(Cell)}};
    sprite.filter = NeuexpEngine::TextureFilter::Nearest;
    sprite.transform.origin = NeuexpEngine::Vector2{Cell / 2.0F, Cell / 2.0F};
    return sprite;
}

Orbiter::Orbiter(NeuexpEngine::Scene& owner, float radius, float phase, float speed, float lifetime)
    : owner(owner)
    , radius(radius)
    , phase(phase)
    , speed(speed)
    , lifetime(lifetime)
{
    const NeuexpEngine::Vector2 center = worldCenter();
    transform().position = NeuexpEngine::Vector2{center.x + std::cos(phase) * radius,
        center.y + std::sin(phase) * radius};
    transform().rotation = phase;
}

void Orbiter::update(float deltaTime)
{
    elapsed += deltaTime;
    const NeuexpEngine::Vector2 center = worldCenter();
    const float angle = phase + elapsed * speed;
    transform().position = NeuexpEngine::Vector2{center.x + std::cos(angle) * radius,
        center.y + std::sin(angle) * radius};
    transform().rotation = angle;
    if (lifetime > 0 && elapsed >= lifetime) {
        owner.destroy(id());
    }
}

Hunter::Hunter(NeuexpEngine::Scene& owner, std::string targetGroup)
    : owner(owner)
    , targetGroup(std::move(targetGroup))
{
}

void Hunter::update(float deltaTime)
{
    // Цель ищется по группе: удалённые объекты в выборку уже не попадают.
    NeuexpEngine::GameObject* target = nullptr;
    float bestDistance = 0;
    for (NeuexpEngine::GameObject* candidate : owner.findByGroup(targetGroup)) {
        const float candidateDistance = distance(transform().position, candidate->transform().position);
        if (!target || candidateDistance < bestDistance) {
            target = candidate;
            bestDistance = candidateDistance;
        }
    }
    if (!target) {
        return;
    }
    if (bestDistance < 12) {
        owner.destroy(target->id());
        return;
    }
    const NeuexpEngine::Vector2 step = normalized(target->transform().position - transform().position)
        * (HunterSpeed * deltaTime);
    transform().position += step;
    transform().rotation = std::atan2(step.y, step.x);
}

Floater::Floater(float baseY, float amplitude, float phase)
    : baseY(baseY)
    , amplitude(amplitude)
    , phase(phase)
{
}

void Floater::update(float deltaTime)
{
    elapsed += deltaTime;
    transform().position.y = baseY + std::sin(elapsed * 1.4F + phase) * amplitude;
    transform().rotation = std::sin(elapsed * 0.8F + phase) * 0.5F;
}

void populateOrbit(NeuexpEngine::Scene& scene, const NeuexpEngine::Texture& texture)
{
    scene.camera().referenceSize = NeuexpEngine::Size2{WorldWidth, WorldHeight};
    scene.camera().position = worldCenter();
    scene.camera().mode = NeuexpEngine::ViewportMode::Fit;

    auto hunter = std::make_unique<Hunter>(scene, "rock");
    hunter->setName("hunter");
    hunter->setGroup("hunter");
    hunter->visual() = makeSprite(texture, Art::Ship);
    hunter->transform().position = worldCenter();
    scene.add(std::move(hunter));

    // Камни истекают и удаляются во время обновления сцены.
    for (int index = 0; index < 22; ++index) {
        const float radius = 60.0F + static_cast<float>(index % 6) * 36.0F;
        const float phase = static_cast<float>(index) * 0.71F;
        const float speed = 0.30F + static_cast<float>(index % 4) * 0.12F;
        const float lifetime = 6.0F + static_cast<float>(index % 7) * 1.4F;
        auto rock = std::make_unique<Orbiter>(scene, radius, phase, speed, lifetime);
        rock->setName("rock-" + std::to_string(index));
        rock->setGroup("rock");
        rock->visual() = makeSprite(texture, Art::Rock);
        scene.add(std::move(rock));
    }

    // Звёзды не истекают: видимость их группы переключается клавишей H.
    for (int index = 0; index < 9; ++index) {
        const float radius = 250.0F + static_cast<float>(index % 3) * 18.0F;
        const float phase = static_cast<float>(index) * 0.87F + 0.35F;
        const float speed = 0.12F + static_cast<float>(index % 2) * 0.07F;
        auto star = std::make_unique<Orbiter>(scene, radius, phase, speed, 0.0F);
        star->setName("star-" + std::to_string(index));
        star->setGroup("star");
        star->visual() = makeSprite(texture, Art::Star);
        scene.add(std::move(star));
    }
}

void populateGrid(NeuexpEngine::Scene& scene, const NeuexpEngine::Texture& texture)
{
    scene.camera().referenceSize = NeuexpEngine::Size2{WorldWidth, WorldHeight};
    scene.camera().position = worldCenter();
    scene.camera().mode = NeuexpEngine::ViewportMode::Fit;

    auto owner = std::make_unique<NeuexpEngine::GameObject>();
    owner->setName("owner");
    owner->setGroup("owner");
    owner->visual() = makeSprite(texture, Art::Ship);
    owner->transform().position = worldCenter();
    owner->transform().scale = NeuexpEngine::Vector2{2, 2};
    scene.add(std::move(owner));

    constexpr int Columns = 8;
    constexpr int Rows = 5;
    for (int row = 0; row < Rows; ++row) {
        for (int column = 0; column < Columns; ++column) {
            const int index = row * Columns + column;
            const float x = 100.0F + static_cast<float>(column) * 82.0F;
            const float y = 140.0F + static_cast<float>(row) * 70.0F;
            auto box = std::make_unique<Floater>(y, 11.0F, static_cast<float>(index) * 0.53F);
            box->setName("box-" + std::to_string(index));
            box->setGroup("box");
            box->visual() = makeSprite(texture, Art::Box);
            box->transform().position = NeuexpEngine::Vector2{x, y};
            scene.add(std::move(box));
        }
    }
}
}
