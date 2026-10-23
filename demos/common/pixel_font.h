#pragma once

#include "math2d.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <array>
#include <string_view>

namespace Demo
{
using Engine::Color;
using Engine::Vector2;

// Надписи демо состоят из прямоугольных пикселей 3x5.
inline void text(std::string_view message, Vector2 position, Color color, float pixel = 2)
{
    constexpr std::array<unsigned int, 36> Glyphs = {
        0b010'101'111'101'101, 0b110'101'110'101'110, 0b011'100'100'100'011,
        0b110'101'101'101'110, 0b111'100'110'100'111, 0b111'100'110'100'100,
        0b011'100'101'101'011, 0b101'101'111'101'101, 0b111'010'010'010'111,
        0b001'001'001'101'010, 0b101'101'110'101'101, 0b100'100'100'100'111,
        0b101'111'111'101'101, 0b101'111'111'111'101, 0b010'101'101'101'010,
        0b110'101'110'100'100, 0b010'101'101'111'011, 0b110'101'110'101'101,
        0b011'100'010'001'110, 0b111'010'010'010'010, 0b101'101'101'101'111,
        0b101'101'101'101'010, 0b101'101'111'111'101, 0b101'101'010'101'101,
        0b101'101'010'010'010, 0b111'001'010'100'111,
        0b111'101'101'101'111, 0b010'110'010'010'111, 0b110'001'111'100'111,
        0b110'001'110'001'110, 0b101'101'111'001'001, 0b111'100'110'001'110,
        0b011'100'111'101'111, 0b111'001'010'010'010, 0b111'101'111'101'111,
        0b111'101'111'001'110
    };
    glColor4f(color.red, color.green, color.blue, color.alpha);
    glBegin(GL_QUADS);
    for (char character : message) {
        unsigned int glyph = 0;
        if (character >= 'A' && character <= 'Z') {
            glyph = Glyphs[static_cast<std::size_t>(character - 'A')];
        } else if (character >= '0' && character <= '9') {
            glyph = Glyphs[26 + static_cast<std::size_t>(character - '0')];
        } else if (character == '-') {
            glyph = 0b000'000'111'000'000;
        } else if (character == '+') {
            glyph = 0b000'010'111'010'000;
        } else if (character == '/') {
            glyph = 0b001'001'010'100'100;
        }
        for (int row = 0; row < 5; ++row) {
            for (int column = 0; column < 3; ++column) {
                if ((glyph & (1U << (14 - row * 3 - column))) != 0) {
                    const float x = position.x + static_cast<float>(column) * pixel;
                    const float y = position.y + static_cast<float>(row) * pixel;
                    glVertex2f(x, y);
                    glVertex2f(x + pixel, y);
                    glVertex2f(x + pixel, y + pixel);
                    glVertex2f(x, y + pixel);
                }
            }
        }
        position.x += 4 * pixel;
    }
    glEnd();
}
}
