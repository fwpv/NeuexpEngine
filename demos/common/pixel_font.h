#pragma once

#include "pixel_glyphs.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <string_view>

namespace Demo
{
using Engine::Color;
using Engine::Vector2;

// Надписи демо состоят из прямоугольных пикселей 3x5.
inline void text(std::string_view message, Vector2 position, Color color, float pixel = 2)
{
    glColor4f(color.red, color.green, color.blue, color.alpha);
    glBegin(GL_QUADS);
    textPixels(message, position, pixel, [](const Engine::Rect& bounds) {
        const float x = bounds.position.x;
        const float y = bounds.position.y;
        glVertex2f(x, y);
        glVertex2f(x + bounds.size.width, y);
        glVertex2f(x + bounds.size.width, y + bounds.size.height);
        glVertex2f(x, y + bounds.size.height);
        return true;
    });
    glEnd();
}
}
