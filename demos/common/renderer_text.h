#pragma once

#include "pixel_glyphs.h"
#include "renderer.h"

namespace Demo
{
inline bool text(Engine::Renderer2D& renderer, std::string_view message, Engine::Vector2 position,
    Engine::Color color, Engine::RenderError& error, float pixel = 2)
{
    Engine::DrawOptions options;
    options.color = color;
    error = Engine::RenderError::None;
    return textPixels(message, position, pixel, [&](const Engine::Rect& bounds) {
        return renderer.drawRectangle(bounds, Engine::ShapeFill::Filled, options, error);
    });
}
}
