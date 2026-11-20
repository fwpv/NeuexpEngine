#include "app/application.h"
#include "app/entry_point.h"
#include "graphics/font.h"
#include "app/input.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace NeuexpEngine;

constexpr std::array<const char*, 3> FontFiles{"cat/font.fnt", "font/fnt14.fnt", "font/fnt20.fnt"};
constexpr std::array<const char*, 3> AlignmentNames{"Left", "Center", "Right"};
constexpr std::array<TextAlignment, 3> Alignments{
    TextAlignment::Left, TextAlignment::Center, TextAlignment::Right};

constexpr std::size_t LabelFont = 1;
constexpr std::size_t LargeFont = 2;
constexpr float ColumnStart = 24;
constexpr float ColumnStep = 356;
constexpr float RowTextY = 125;
constexpr float RowLabelY = 215;
constexpr float WrappedTextY = 248;

constexpr Color White{0.9F, 0.95F, 1};
constexpr Color Cyan{0.3F, 0.85F, 1};

struct Sample
{
    TextLayout text;
    Vector2 position;
    Color color;
};

class FontApplication final : public Application
{
private:
    bool addSample(const BitmapFont& font, const std::string& text, Vector2 position,
        Color color, Platform::Error& error, const TextLayoutOptions& options = {})
    {
        Sample sample;
        sample.position = position;
        sample.color = color;
        ResourceError failure;
        if (!font.layout(text, sample.text, failure, options)) {
            error = {"Font demo", failure.message, {}};
            return false;
        }
        samples.push_back(std::move(sample));
        return true;
    }

    bool loadFonts(Platform::Error& error)
    {
        ResourceFiles files;
        ResourceError failure;
        if (!files.initialize(failure)) {
            error = {"Font demo", failure.message, {}};
            return false;
        }
        for (std::size_t index = 0; index < fonts.size(); ++index) {
            if (!getRenderer().loadFont(files, FontFiles[index], fonts[index], failure)) {
                error = {"Font demo", failure.message, {}};
                return false;
            }
        }
        return true;
    }

    bool addHeaders(Platform::Error& error)
    {
        if (!addSample(fonts[LargeFont], u8"Растровые шрифты / Bitmap fonts", {24, 15}, White, error)) {
            return false;
        }
        if (!addSample(fonts[LabelFont],
                u8"F: фильтрация   B: рамки   Space: движение   Esc: выход",
                {24, 60}, Cyan, error)) {
            return false;
        }
        ResourceError failure;
        if (!fonts[LabelFont].layout("Linear", filters[0], failure)
            || !fonts[LabelFont].layout("Nearest", filters[1], failure)) {
            error = {"Font demo", failure.message, {}};
            return false;
        }
        return true;
    }

    bool addFontRows(Platform::Error& error)
    {
        for (std::size_t index = 0; index < fonts.size(); ++index) {
            const float x = ColumnStart + static_cast<float>(index) * ColumnStep;
            const std::string text = std::string(FontFiles[index]) + u8"\nПривет! Ёж / Hello 0123";
            if (!addSample(fonts[index], text, {x, RowTextY}, White, error)) {
                return false;
            }
            if (!addSample(fonts[LabelFont], AlignmentNames[index], {x, RowLabelY}, Cyan, error)) {
                return false;
            }
            const TextLayoutOptions wrapped{330, 1, 4, Alignments[index], true};
            if (!addSample(fonts[LabelFont],
                    u8"Перенос по словам: русский и English text.\n"
                    u8"Сверхдлинноесловобезпробеловдляпроверкипереноса.",
                    {x, WrappedTextY}, White, error, wrapped)) {
                return false;
            }
        }
        return true;
    }

    bool addEffects(Platform::Error& error)
    {
        if (!addSample(fonts[LabelFont], u8"Масштаб 1.5 / Scale", {24, 410},
                Color{1, 0.75F, 0.3F}, error, TextLayoutOptions{0, 1.5F, 0, TextAlignment::Left})) {
            return false;
        }
        if (!addSample(fonts[LabelFont], u8"Цвет и прозрачность / Alpha 0.5", {580, 410},
                Color{1, 0.4F, 0.6F, 0.5F}, error)) {
            return false;
        }
        if (!addSample(fonts[LabelFont],
                u8"  Отступ + CRLF\r\n\r\nПустая строка; интервал +6",
                {24, 470}, White, error, TextLayoutOptions{0, 1, 6, TextAlignment::Left})) {
            return false;
        }
        if (!addSample(fonts[LargeFont],
                "j: " + std::string(u8"смещение глифа\nFallback: 😀 / UTF-8: ") + '\xff',
                {580, 470}, White, error)) {
            return false;
        }
        if (!addSample(fonts[LabelFont],
                u8"Поворот, движение и отсечение текста",
                {24, 585}, Cyan, error)) {
            return false;
        }

        ResourceError failure;
        if (!fonts[LabelFont].layout(u8"Поворот / Rotate\nОтсечение / Clip", animated, failure,
                TextLayoutOptions{270, 1.5F, 3, TextAlignment::Center, true})) {
            error = {"Font demo", failure.message, {}};
            return false;
        }
        if (!addSample(fonts[LabelFont],
                u8"Зелёная рамка: логический размер. Розовая: границы глифов.",
                {24, 780}, Cyan, error)) {
            return false;
        }

        const Size2 size = animated.size();
        return addSample(fonts[LabelFont],
            "Measured: " + std::to_string(static_cast<int>(size.width)) + " x "
                + std::to_string(static_cast<int>(size.height)) + " / lines: "
                + std::to_string(animated.lineCount()),
            {24, 815}, White, error);
    }

    bool onStart(Platform::Error& error) override
    {
        if (!initializeRenderer(error) || !loadFonts(error)) {
            return false;
        }
        return addHeaders(error) && addFontRows(error) && addEffects(error);
    }

    bool onInput(const FrameInfo&, Platform::Error&) override
    {
        const auto& input = Platform::getInput();
        if (input.key(Key::Escape).pressed) {
            requestExit();
        }
        if (input.key(Key::F).pressed) {
            nearest = !nearest;
        }
        if (input.key(Key::B).pressed) {
            bounds = !bounds;
        }
        if (input.key(Key::Space).pressed) {
            moving = !moving;
        }
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
        if (!bounds) {
            return;
        }
        DrawOptions outline = options;
        outline.color = {0.25F, 0.7F, 0.4F, 0.7F};
        renderer.drawRectangle(Rect{position, text.size()}, outline, ShapeFill::Outline);
        outline.color = {0.9F, 0.4F, 0.6F, 0.7F};
        const auto ink = text.inkBounds();
        renderer.drawRectangle(Rect{position + ink.position, ink.size}, outline, ShapeFill::Outline);
    }

    bool makeView(const FrameInfo& frame, UiView& view, Platform::Error& error)
    {
        const float scale = std::min(static_cast<float>(frame.window.width) / 1100,
            static_cast<float>(frame.window.height) / 860);
        UiError failure;
        if (!UiCanvas{scale}.createView(
                Viewport{0, 0, frame.window.width, frame.window.height}, view, failure)) {
            error = {"Font demo", "Invalid viewport", {}};
            return false;
        }
        return true;
    }

    bool onRender(const FrameInfo& frame, Platform::Error& error) override
    {
        UiView view;
        if (!makeView(frame, view, error)) {
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
