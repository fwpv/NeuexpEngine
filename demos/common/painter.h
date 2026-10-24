#pragma once

#include "application.h"
#include "renderer_text.h"

#include <string>
#include <string_view>
#include <vector>

namespace Demo
{
// Общие операции демо сохраняют первую ошибку до завершения кадра.
class Painter
{
public:
    explicit Painter(Engine::Renderer2D& target)
        : renderer(target)
    {
    }

    Engine::DrawOptions options;

    bool initialize(Platform::Error& error)
    {
        success = renderer.initialize(renderError);
        return result(error);
    }

    void beginFrame(const Engine::FrameInfo& frame, Engine::Color clear)
    {
        options = {};
        success = renderer.beginFrame(frame.window.width, frame.window.height, clear, renderError);
        frameStarted = success;
    }

    void beginWorld(const Engine::CameraView& view)
    {
        if (success) { success = renderer.beginWorld(view, renderError); }
    }

    void beginUi(const Engine::UiView& view)
    {
        if (success) { success = renderer.beginUi(view, renderError); }
    }

    void rectangle(const Engine::Rect& bounds, bool filled = true)
    {
        if (success) {
            success = renderer.drawRectangle(bounds, filled ? Engine::ShapeFill::Filled : Engine::ShapeFill::Outline,
                options, renderError);
        }
    }

    void rectangle(const Engine::Rect& bounds, Engine::Color color, bool filled = true)
    {
        options.color = color;
        rectangle(bounds, filled);
    }

    void circle(const Engine::Circle& shape, bool filled = true, unsigned int segments = 48)
    {
        if (success) {
            success = renderer.drawCircle(shape, filled ? Engine::ShapeFill::Filled : Engine::ShapeFill::Outline,
                options, renderError, segments);
        }
    }

    void circle(const Engine::Circle& shape, Engine::Color color, bool filled = true, unsigned int segments = 48)
    {
        options.color = color;
        circle(shape, filled, segments);
    }

    void polygon(const std::vector<Engine::Vector2>& vertices, bool filled = true)
    {
        if (success) {
            success = renderer.drawPolygon(vertices, filled ? Engine::ShapeFill::Filled : Engine::ShapeFill::Outline,
                options, renderError);
        }
    }

    void line(Engine::Vector2 start, Engine::Vector2 end)
    {
        if (success) { success = renderer.drawLine(start, end, options, renderError); }
    }

    void point(Engine::Vector2 position)
    {
        if (success) { success = renderer.drawPoint(position, options, renderError); }
    }

    void gradient(const Engine::Rect& bounds, Engine::Color top, Engine::Color bottom)
    {
        if (success) { success = renderer.drawGradientRectangle(bounds, top, bottom, options, renderError); }
    }

    void text(std::string_view message, Engine::Vector2 position, Engine::Color color, float pixel = 2)
    {
        if (success) { success = Demo::text(renderer, message, position, color, renderError, pixel); }
    }

    bool endFrame(Platform::Error& error)
    {
        if (frameStarted) {
            Engine::RenderError endError;
            const bool ended = renderer.endFrame(endError);
            if (success && !ended) {
                success = false;
                renderError = endError;
            }
            frameStarted = false;
        }
        return result(error);
    }

private:
    Engine::Renderer2D& renderer;
    Engine::RenderError renderError = Engine::RenderError::None;
    bool success = true;
    bool frameStarted = false;

    bool result(Platform::Error& error) const
    {
        if (!success) {
            error = { "Renderer2D", "Rendering failed (RenderError="
                + std::to_string(static_cast<int>(renderError)) + ").", {} };
        }
        return success;
    }
};
}
