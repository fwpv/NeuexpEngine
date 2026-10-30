#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>
#include "game.h"
#include "renderer.h"
#include "platform.h"
#include <GL/gl.h>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void render()
{
    Engine::Renderer2D renderer;
    Engine::RenderError renderError;
    require(renderer.initialize(renderError), "Renderer initialization failed");
    Cat::Images images;
    Engine::ResourceError resourceError;
    require(images.load(renderer, resourceError), resourceError.message.c_str());
    for (const auto size : {Engine::Size2{935, 700}, Engine::Size2{1200, 700}, Engine::Size2{935, 900}}) {
        Cat::Game game(42, 123);
#ifdef REFACTORED
        auto& world = game.world;
        auto& foliage = game.foliage;
#else
        auto& world = game;
        auto& foliage = game;
#endif
        // Заданные слои и лифт гарантируют видимое пересечение спрайтов.
        world.addPlatform(560, 400, Cat::Turf);
        world.decorate(world.platforms.back());
        world.addPlatform(800, 420, Cat::Lift, 200, 600);
        const int width = static_cast<int>(size.width);
        const int height = static_cast<int>(size.height);
        for (int frame = 0; frame < 600; ++frame) {
            game.update(0.01F, Cat::Controls{frame < 400, frame >= 400, frame % 71 == 0, frame % 173 == 0});
            Engine::CameraError cameraError;
            require(game.prepareView(width, height, cameraError), "Camera preparation failed");
            if (frame % 50 != 0) {
                continue;
            }
            // Проверка порядка листьев относительно кота и платформ.
            foliage.leaves[0].active = true;
            foliage.leaves[0].position = world.cat.position() - Engine::Vector2{10, 20};
            renderer.beginFrame(width, height, Engine::Color{245.0F / 255, 250.0F / 255, 245.0F / 255});
            game.draw(renderer, images);
            require(renderer.endFrame(renderError), "Frame rendering failed");
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
            glReadBuffer(GL_BACK);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            require(glGetError() == GL_NO_ERROR, "Pixel read failed");
            require(!std::equal(pixels.begin() + 4, pixels.end(), pixels.begin()), "Empty uniform framebuffer");
            std::cout.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
        }
    }
    renderer.shutdown();
}
}

int main()
{
    int result = 1;
    try {
        Platform::WindowOptions options;
        options.width = 1200;
        options.height = 900;
        options.title = "Cat equivalence diagnostic";
        std::vector<Platform::Error> errors;
        require(Platform::initialize(options, errors), "Platform initialization failed");
        render();
        result = 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    std::vector<Platform::Error> errors;
    Platform::shutdown(errors);
    return errors.empty() ? result : 1;
}
