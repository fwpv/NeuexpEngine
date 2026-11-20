#include "scene.h"
#include "platform.h"
#include "renderer.h"
#include "render_target.h"
#include "resources.h"
#include "sprite.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
using namespace NeuexpEngine;
using RenderTest::require;

constexpr int Size = 128;
constexpr float Center = Size / 2.0F;

std::array<unsigned char, 3> pixel(int x, int y)
{
    std::array<unsigned char, 3> result{};
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, Size - 1 - y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, result.data());
    require(glGetError() == GL_NO_ERROR, "Read pixels failed");
    return result;
}

// Объект, который рисует собственный прямоугольник, а не спрайт.
class Marker : public GameObject
{
public:
    void draw(Renderer2D& renderer) const override
    {
        DrawOptions options;
        options.transform = transform().toAffine();
        renderer.drawRectangle(Rect{Vector2{}, Size2{16, 16}}, options);
    }
};

struct Placement
{
    Vector2 position;
    Rect textureRect;
    Size2 size;
};

std::unique_ptr<GameObject> makeSprite(const Texture& texture, Placement placement)
{
    auto object = std::make_unique<GameObject>();
    object->visual() = Sprite{texture, placement.textureRect};
    object->visual().size = placement.size;
    object->visual().filter = TextureFilter::Nearest;
    object->transform().position = placement.position;
    return object;
}

void draw(Scene& scene, Renderer2D& renderer)
{
    CameraView view;
    CameraError cameraError;
    require(scene.createView(Size, Size, view, cameraError), "Scene view failed");
    renderer.beginFrame(Size, Size, Color{0, 0, 0, 1});
    renderer.setWorldView(view);
    scene.draw(renderer);
    RenderError renderError;
    require(renderer.endFrame(renderError), "Scene frame failed");
    require(renderError == RenderError::None, "Scene frame reported an error");
}

const Rect Red{0, 0, 1, 1};
const Rect Green{1, 0, 1, 1};
const Size2 Block{32, 32};

void run()
{
    RenderTest::FramebufferApi api;
    require(api.load(), "Pixel tests require FBO support");
    RenderTest::RenderTarget target(api, Size);
    target.initialize();
    Renderer2D renderer;
    RenderError renderError;
    require(renderer.initialize(renderError), "Renderer init failed");

    Image image{2, 1, std::vector<std::uint8_t>{255, 0, 0, 255, 0, 255, 0, 255}};
    Texture texture;
    ResourceError resourceError;
    require(renderer.createTexture(image, texture, resourceError), "Texture creation failed");

    Scene scene;
    scene.camera().referenceSize = Size2{Center * 2, Center * 2};
    scene.camera().position = Vector2{Center, Center};

    auto firstObject = makeSprite(texture, Placement{Vector2{0, 0}, Red, Block});
    GameObject* first = firstObject.get();
    scene.add(std::move(firstObject));
    auto secondObject = makeSprite(texture, Placement{Vector2{16, 16}, Green, Block});
    scene.add(std::move(secondObject));
    auto hiddenObject = makeSprite(texture, Placement{Vector2{64, 0}, Red, Block});
    GameObject* hidden = hiddenObject.get();
    scene.add(std::move(hiddenObject));
    auto removedObject = makeSprite(texture, Placement{Vector2{96, 0}, Red, Block});
    const ObjectId removedId = scene.add(std::move(removedObject));
    auto plainObject = std::make_unique<GameObject>();
    plainObject->transform().position = Vector2{0, 96};
    scene.add(std::move(plainObject));
    auto markerObject = std::make_unique<Marker>();
    markerObject->transform().position = Vector2{96, 64};
    scene.add(std::move(markerObject));

    draw(scene, renderer);
    require(pixel(8, 8) == std::array<unsigned char, 3>{255, 0, 0}, "Sprite ignored object position");
    require(pixel(20, 20) == std::array<unsigned char, 3>{0, 255, 0}, "Later object must be drawn on top");
    require(pixel(80, 16) == std::array<unsigned char, 3>{255, 0, 0}, "Second sprite missing");
    require(pixel(104, 8) == std::array<unsigned char, 3>{255, 0, 0}, "Third sprite missing");
    require(pixel(100, 68) == std::array<unsigned char, 3>{255, 255, 255}, "Custom draw override missing");
    require(pixel(48, 48) == std::array<unsigned char, 3>{0, 0, 0}, "Background must stay between objects");

    hidden->setVisible(false);
    scene.destroy(removedId);
    draw(scene, renderer);
    require(pixel(80, 16) == std::array<unsigned char, 3>{0, 0, 0}, "Hidden object must not be drawn");
    require(pixel(104, 8) == std::array<unsigned char, 3>{0, 0, 0}, "Object pending removal must not be drawn");
    require(pixel(8, 8) == std::array<unsigned char, 3>{255, 0, 0}, "Visible neighbour lost after removal");
    require(pixel(20, 20) == std::array<unsigned char, 3>{0, 255, 0}, "Visible neighbour order lost");

    // Удаление экземпляра не освобождает общую текстуру: ею ещё пользуется другой объект.
    scene.destroy(first->id());
    require(texture.isValid(), "Shared texture must survive the instance");
    draw(scene, renderer);
    require(texture.isValid() && renderer.textureStats().count == 1, "Shared texture must stay loaded");
    require(pixel(20, 20) == std::array<unsigned char, 3>{0, 255, 0}, "Shared texture stopped drawing");
    require(pixel(8, 8) == std::array<unsigned char, 3>{0, 0, 0}, "Removed instance still drawn");

    scene.clear();
    draw(scene, renderer);
    require(pixel(20, 20) == std::array<unsigned char, 3>{0, 0, 0}, "Clear must stop drawing objects");
    require(texture.isValid(), "Texture must stay valid after clearing the scene");
    renderer.shutdown();
}
}

int main()
{
#ifndef _WIN32
    const auto display = std::getenv("DISPLAY");
    if (!display || !*display) {
        return 77;
    }
#endif
    int result = 1;
    Platform::Error failure;
    Platform::WindowOptions options;
    options.width = Size;
    options.height = Size;
    options.title = "Scene pixel tests";
    if (!Platform::initialize(options, failure)) {
        Platform::reportError(failure, false);
        return 1;
    }
    try {
        run();
        result = 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
    }
    if (!Platform::shutdown(failure)) {
        Platform::reportError(failure, false);
        return 1;
    }
    return result;
}
