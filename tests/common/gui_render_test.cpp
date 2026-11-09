#include "gui/context.h"
#include "platform.h"
#include "render_target.h"
#include "detail/unicode.h"

#include <array>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
using namespace Engine;
using RenderTest::require;
namespace UI = Engine::Gui;
constexpr int Size = 256;
std::array<unsigned char, 3> pixel(int x, int y)
{
    std::array<unsigned char, 3> result{};
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, Size - 1 - y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, result.data());
    require(glGetError() == GL_NO_ERROR, "Read pixels failed");
    return result;
}
void draw(UI::Context& context, Renderer2D& renderer)
{
    renderer.beginFrame(Size, Size, Color{0, 0, 0, 1});
    ResourceError failure;
    const bool drawn = context.draw(renderer, failure);
    require(drawn, failure.message.c_str());
    RenderError error;
    require(renderer.endFrame(error), "GUI render failed");
}
void borders(Renderer2D& renderer)
{
    UI::Context context;
    auto page = std::make_shared<UI::Screen>();
    auto button = std::make_shared<UI::Button>();
    button->style().background = Color{0, 0, 1, 1};
    button->style().border = Color{1, 0, 0, 0.5F};
    page->add(button);
    context.setScreen(page);
    std::vector<unsigned char> pixels(Size * Size * 3);
    // Проверяется весь периметр, в том числе углы, при дробной компоновке.
    for (float scale : {0.75F, 1.0F, 1.25F, 1.5F, 2.0F}) {
        for (int viewportSize : {240, 241}) {
            context.setViewport(Viewport{3, 5, viewportSize, viewportSize}, scale);
            for (float offset : {0.0F, 0.25F, 0.5F, 0.75F}) {
                UI::Placement placement;
                placement.size = Size2{31, 17};
                placement.horizontal = UI::Align::Center;
                placement.vertical = UI::Align::Center;
                placement.offset = Vector2{offset, offset};
                button->setPlacement(placement);
                draw(context, renderer);
                // Читаем кадр целиком, чтобы не синхронизироваться с GPU на каждом пикселе.
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadPixels(0, 0, Size, Size, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
                require(glGetError() == GL_NO_ERROR, "Read border pixels failed");
                const auto bounds = *context.bounds(*button);
                const int left = 3 + static_cast<int>(std::ceil(bounds.position.x));
                const int top = 5 + static_cast<int>(std::ceil(bounds.position.y));
                const int right = 3 + static_cast<int>(std::ceil(bounds.position.x + bounds.size.width));
                const int bottom = 5 + static_cast<int>(std::ceil(bounds.position.y + bounds.size.height));
                for (int y = top - 1; y <= bottom; ++y) {
                    for (int x = left - 1; x <= right; ++x) {
                        const auto actual = pixels.data() + ((Size - 1 - y) * Size + x) * 3;
                        const bool outside = x < left || x >= right || y < top || y >= bottom;
                        const bool border = !outside && (x == left || x == right - 1 || y == top || y == bottom - 1);
                        if (border) {
                            // Полупрозрачные углы не должны рисоваться дважды.
                            require(actual[0] >= 126 && actual[0] <= 129,
                                "GUI border missing, thicker than one pixel, or blended twice");
                        } else {
                            require(actual[0] == 0, "GUI border escaped its one-pixel perimeter");
                            if (!outside) {
                                require(actual[2] == 255, "GUI border covered the interior");
                            }
                        }
                    }
                }
            }
        }
    }
}
void run()
{
    RenderTest::FramebufferApi api;
    require(api.load(), "Pixel tests require FBO support");
    RenderTest::RenderTarget target(api, Size);
    target.initialize();
    Renderer2D renderer;
    RenderError error;
    require(renderer.initialize(error), "Renderer init failed");
    UI::Context context;
    context.setViewport(Viewport{0, 0, Size, Size});
    auto page = std::make_shared<UI::Screen>();
    auto first = std::make_shared<UI::Button>();
    auto second = std::make_shared<UI::Button>();
    first->style().background = Color{1, 0, 0, 1};
    second->style().background = Color{0, 0, 1, 1};
    UI::Placement placement;
    placement.size = Size2{80, 40};
    first->setPlacement(placement);
    placement.offset = Vector2{40, 20};
    second->setPlacement(placement);
    page->add(first);
    page->add(second);
    context.setScreen(page);
    draw(context, renderer);
    require(pixel(10, 10)[0] == 255 && pixel(50, 30)[2] == 255, "GUI paint order wrong");
    int clicks = 0;
    auto subscription = second->subscribe(UI::EventType::Click, [&](const UI::Event&) { ++clicks; });
    Engine::Event event;
    event.button = MouseButton::Left;
    event.x = 50;
    event.y = 30;
    event.type = Engine::EventType::MousePressed;
    context.handle(event);
    event.type = Engine::EventType::MouseReleased;
    context.handle(event);
    require(clicks == 1, "Visual top did not receive click");

    Texture texture;
    ResourceError failure;
    require(renderer.createTexture(Image{2, 1, std::vector<std::uint8_t>{255, 0, 0, 255, 0, 255, 0, 255}}, texture, failure),
        "Cannot create skin texture");
    second->style().images[0] = Sprite{texture, Rect{0, 0, 1, 1}};
    second->style().images[1] = Sprite{texture, Rect{1, 0, 1, 1}};
    second->style().images[0].filter = TextureFilter::Nearest;
    second->style().images[1].filter = TextureFilter::Nearest;
    draw(context, renderer);
    require(pixel(50, 30)[1] == 255, "Hovered sprite state not rendered");
    second->setEnabled(false);
    draw(context, renderer);
    require(pixel(110, 50)[0] > 90 && pixel(110, 50)[0] < 115, "Disabled image fallback failed");

    page->clear();
    auto group = std::make_shared<UI::Container>();
    placement = UI::Placement{};
    placement.size = Size2{40, 40};
    group->setPlacement(placement);
    first->style().hover = first->style().background;
    group->add(first);
    page->add(group);
    draw(context, renderer);
    require(pixel(20, 20)[0] == 255 && pixel(50, 20)[0] == 0, "Child paint escaped container clip");

    page->clear();
    auto label = std::make_shared<UI::Label>(u8"Яё");
    placement = UI::Placement{};
    placement.size = Size2{22, 14};
    label->setPlacement(placement);
    label->style().padding = 0;
    label->style().foreground = Color{1, 1, 1, 1};
    page->add(label);
    draw(context, renderer);
    require(pixel(2, 0)[0] == 255, "Cyrillic glyph not rendered");
    require(pixel(0, 0)[0] == 0 && pixel(22, 0)[0] == 0, "Cyrillic bounds incorrect");
    const auto uppercase = pixel(2, 0);
    label->setText(u8"яЁ");
    draw(context, renderer);
    require(pixel(2, 0) == uppercase, "Cyrillic case folding failed");

    // Все буквы и цифры используют полную высоту 5x7 и дают видимые примитивы.
    const std::string alphabet = u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
        u8"АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ";
    for (std::size_t offset = 0; offset < alphabet.size();) {
        std::uint32_t code;
        Detail::nextUtf8(alphabet, offset, code);
        std::string letter;
        Detail::appendUtf8(letter, code);
        require(Renderer2D::measureDebugText(letter, 1) == Size2{5, 7}, "Inconsistent debug glyph cell");
        label->setText(letter);
        draw(context, renderer);
        bool visible = false;
        for (int y = 0; y < 14; ++y) {
            for (int x = 0; x < 22; ++x) { visible = visible || pixel(x, y)[0] > 0; }
        }
        require(visible, "Missing debug glyph");
    }
    // Растровый шрифт используется тем же компонентом, включая масштаб и выравнивание.
    BitmapFont font;
    Texture mask;
    require(renderer.createTexture(Image{1, 1, std::vector<std::uint8_t>{255, 255, 255, 255}}, mask, failure), "Mask creation failed");
    const std::string description = "bitmap_font 1\ntexture unused.png\nlineHeight 1\nfallback 65\n"
        "glyph 32 0 0 0 0 0 0 1\nglyph 65 0 0 1 1 0 0 1\n";
    const bool loaded = font.load(mask, description, failure);
    require(loaded, failure.message.c_str());
    label->style().font = font;
    label->style().textScale = 10;
    label->setText("A");
    draw(context, renderer);
    require(pixel(10, 6)[0] > 200, "Bitmap label missing");
    require(renderer.createTexture(Image{2, 1, std::vector<std::uint8_t>{
        255, 255, 255, 255, 255, 255, 255, 0}}, mask, failure), "Filter mask creation failed");
    require(font.load(mask, "bitmap_font 1\ntexture unused.png\nlineHeight 1\nfallback 65\n"
        "glyph 32 0 0 0 0 0 0 1\nglyph 65 0 0 2 1 0 0 2\n", failure), "Filter font creation failed");
    label->style().font = font;
    label->style().textFilter = TextureFilter::Nearest;
    draw(context, renderer);
    require(pixel(10, 6)[0] == 255 && pixel(11, 6)[0] == 0, "Nearest text filter ignored");
    label->style().textFilter = TextureFilter::Linear;
    draw(context, renderer);
    require(pixel(10, 6)[0] > 100 && pixel(10, 6)[0] < 180
        && pixel(11, 6)[0] > 80 && pixel(11, 6)[0] < 150, "Linear text filter ignored");
    borders(renderer);
    renderer.shutdown();
}
}
int main()
{
#ifndef _WIN32
    const auto display = std::getenv("DISPLAY");
    if (!display || !*display) { return 77; }
#endif
    int result = 1;
    Platform::Error failure;
    Platform::WindowOptions options;
    options.width = Size;
    options.height = Size;
    options.title = "GUI pixel tests";
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
