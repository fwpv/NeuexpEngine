#include "application.h"
#include "entry_point.h"
#include "font.h"
#include "input.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include <utility>
#include <optional>

namespace
{
using namespace NeuexpEngine;

struct Sample
{
    TextLayout text;
    Vector2 position;
    Color color;
};

class FontApplication final : public Application
{
private:
    bool add(const BitmapFont& font, const std::string& value, Vector2 position,
        const TextLayoutOptions& options, Color color, Platform::Error& error)
    {
        Sample sample;
        sample.position = position;
        sample.color = color;
        ResourceError failure;
        if (!font.layout(value, sample.text, failure, options)) {
            error = {"Font demo", failure.message, {}};
            return false;
        }
        samples.push_back(std::move(sample));
        return true;
    }

    bool onStart(Platform::Error& error) override
    {
        if (!initializeRenderer(error)) {
            return false;
        }
        ResourceFiles files;
        ResourceError failure;
        if (!files.initialize(failure)) {
            error = {"Font demo", failure.message, {}};
            return false;
        }
        const std::array<std::string, 3> names{"cat/font.fnt", "font/fnt14.fnt", "font/fnt20.fnt"};
        for (std::size_t index = 0; index < fonts.size(); ++index) {
            if (!getRenderer().loadFont(files, names[index], fonts[index], failure)) {
                error = {"Font demo", failure.message, {}};
                return false;
            }
        }
        const Color white{0.9F, 0.95F, 1};
        const Color cyan{0.3F, 0.85F, 1};
        if (!add(fonts[2], u8"Растровые шрифты / Bitmap fonts", {24, 15}, {}, white, error)
            || !add(fonts[1], u8"F: фильтрация   B: рамки   Space: движение   Esc: выход",
                {24, 60}, {}, cyan, error)
            || !fonts[1].layout("Linear", filters[0], failure)
            || !fonts[1].layout("Nearest", filters[1], failure)) {
            if (!failure.message.empty()) {
                error = {"Font demo", failure.message, {}};
            }
            return false;
        }
        for (std::size_t index = 0; index < fonts.size(); ++index) {
            const float x = 24 + static_cast<float>(index) * 356;
            if (!add(fonts[index], names[index] + u8"\nПривет! Ёж / Hello 0123", {x, 125}, {}, white, error)) {
                return false;
            }
            const auto alignment = index == 0 ? TextAlignment::Left
                : index == 1 ? TextAlignment::Center : TextAlignment::Right;
            if (!add(fonts[1], index == 0 ? "Left" : index == 1 ? "Center" : "Right",
                    {x, 215}, {}, cyan, error)
                || !add(fonts[1], u8"Перенос по словам: русский и English text.\n"
                    u8"Сверхдлинноесловобезпробеловдляпроверкипереноса.",
                    {x, 248}, TextLayoutOptions{330, 1, 4, alignment, true}, white, error)) {
                return false;
            }
        }
        if (!add(fonts[1], u8"Масштаб 1.5 / Scale", {24, 410}, TextLayoutOptions{0, 1.5F, 0, TextAlignment::Left},
                Color{1, 0.75F, 0.3F}, error)
            || !add(fonts[1], u8"Цвет и прозрачность / Alpha 0.5", {580, 410}, {}, Color{1, 0.4F, 0.6F, 0.5F}, error)
            || !add(fonts[1], u8"  Отступ + CRLF\r\n\r\nПустая строка; интервал +6", {24, 470},
                TextLayoutOptions{0, 1, 6, TextAlignment::Left}, white, error)
            || !add(fonts[2], "j: " + std::string(u8"смещение глифа\nFallback: 😀 / UTF-8: ") + '\xff',
                {580, 470}, {}, white, error)
            || !add(fonts[1], u8"Поворот, движение и отсечение текста", {24, 585}, {}, cyan, error)
            || !fonts[1].layout(u8"Поворот / Rotate\nОтсечение / Clip", animated, failure,
                TextLayoutOptions{270, 1.5F, 3, TextAlignment::Center, true})
            || !add(fonts[1], u8"Зелёная рамка: логический размер. Розовая: границы глифов.",
                {24, 780}, {}, cyan, error)) {
            if (!failure.message.empty()) {
                error = {"Font demo", failure.message, {}};
            }
            return false;
        }
        // Числа получены из того же результата, который будет нарисован.
        const auto size = animated.size();
        return add(fonts[1], "Measured: " + std::to_string(static_cast<int>(size.width)) + " x "
            + std::to_string(static_cast<int>(size.height)) + " / lines: " + std::to_string(animated.lineCount()),
            {24, 815}, {}, white, error);
    }

    bool onInput(const FrameInfo&, Platform::Error&) override
    {
        const auto& input = Platform::getInput();
        if (input.key(Key::Escape).pressed) { requestExit(); }
        if (input.key(Key::F).pressed) { nearest = !nearest; }
        if (input.key(Key::B).pressed) { bounds = !bounds; }
        if (input.key(Key::Space).pressed) { moving = !moving; }
        return true;
    }

    bool onUpdate(const FrameInfo& frame, Platform::Error&) override
    {
        if (moving) {
            time += frame.deltaTime;
        }
        return true;
    }

    void draw(const TextLayout& text, Vector2 position, const DrawOptions& options)
    {
        auto& renderer = getRenderer();
        renderer.drawText(text, position, options, nearest ? TextureFilter::Nearest : TextureFilter::Linear);
        if (bounds) {
            DrawOptions outline = options;
            outline.color = {0.25F, 0.7F, 0.4F, 0.7F};
            renderer.drawRectangle(Rect{position, text.size()}, outline, ShapeFill::Outline);
            outline.color = {0.9F, 0.4F, 0.6F, 0.7F};
            const auto ink = text.inkBounds();
            renderer.drawRectangle(Rect{position + ink.position, ink.size}, outline, ShapeFill::Outline);
        }
    }

    bool onRender(const FrameInfo& frame, Platform::Error& error) override
    {
        const float scale = std::min(static_cast<float>(frame.window.width) / 1100,
            static_cast<float>(frame.window.height) / 860);
        UiView view;
        UiError failure;
        if (!UiCanvas{scale}.createView(Viewport{0, 0, frame.window.width, frame.window.height}, view, failure)) {
            error = {"Font demo", "Invalid viewport", {}};
            return false;
        }
        beginFrame(frame.window.width, frame.window.height, Color{0.035F, 0.055F, 0.09F});
        auto& renderer = getRenderer();
        renderer.setUiView(view);
        for (const auto& sample : samples) {
            DrawOptions options;
            options.color = sample.color;
            draw(sample.text, sample.position, options);
        }
        renderer.drawText(filters[nearest ? 1 : 0], Vector2{580, 92});
        const Rect clip{24, 625, 1052, 130};
        renderer.drawRectangle(clip, Color{0.1F, 0.15F, 0.22F});
        const auto first = view.uiToScreen(clip.position);
        const auto last = view.uiToScreen(clip.position + Vector2{clip.size.width, clip.size.height});
        renderer.setClip(Viewport{static_cast<int>(first.x), static_cast<int>(first.y),
            static_cast<int>(last.x - first.x), static_cast<int>(last.y - first.y)});
        DrawOptions options;
        options.transform = Transform2{Vector2{484, 682}, 0.25F * std::sin(time),
            Vector2{1, 1}, Vector2{135, 45}}.toAffine();
        draw(animated, Vector2{400 * std::sin(time), 0}, options);
        renderer.setClip(std::nullopt);
        return endFrame(error);
    }

    std::array<BitmapFont, 3> fonts;
    std::vector<Sample> samples;
    std::array<TextLayout, 2> filters;
    TextLayout animated;
    float time = 0;
    bool nearest = false;
    bool bounds = false;
    bool moving = true;
};
}

int NeuexpEngine::applicationMain()
{
    FontApplication app;
    ApplicationOptions options;
    options.window.width = 1100;
    options.window.height = 860;
    options.window.title = "Bitmap fonts / Text layout";
    Platform::Error error;
    if (!app.run(options, error)) {
        Platform::reportError(error);
        return 1;
    }
    return 0;
}
