#include "application.h"
#include "transform.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr Engine::Rect LocalBounds = { 0, 0, 160, 100 };
constexpr Engine::Color IdleColor = { 0.15F, 0.45F, 0.8F };
constexpr Engine::Color HitColor = { 0.2F, 0.85F, 0.35F };
constexpr Engine::Color BoundsColor = { 1, 0.6F, 0.2F };
constexpr Engine::Color CursorColor = { 1, 1, 1 };
constexpr Engine::Color CursorBoundsColor = { 1, 1, 0 };
constexpr Engine::Color CursorHitColor = { 1, 0, 0 };
constexpr Engine::Color DirectionColor = { 1, 1, 1 };
constexpr Engine::Rect DirectionShaft = { 28, 44, 80, 12 };
constexpr std::array<Engine::Vector2, 3> DirectionHead = {{ { 132, 50 }, { 108, 34 }, { 108, 66 } }};
constexpr float MovementSpeed = 200;
constexpr float RotationSpeed = Engine::Pi / 2;
constexpr float CursorRadius = 12;
constexpr int CircleSegments = 32;

void setColor(Engine::Color color)
{
    glColor4f(color.red, color.green, color.blue, color.alpha);
}

void drawRectangle(const Engine::Rect& rectangle, const Engine::Affine2& transform, GLenum mode)
{
    const std::array<Engine::Vector2, 4> corners = {{
        rectangle.position,
        { rectangle.position.x + rectangle.size.width, rectangle.position.y },
        { rectangle.position.x + rectangle.size.width, rectangle.position.y + rectangle.size.height },
        { rectangle.position.x, rectangle.position.y + rectangle.size.height }
    }};
    glBegin(mode);
    for (Engine::Vector2 corner : corners) {
        const Engine::Vector2 point = transform.transformPoint(corner);
        glVertex2f(point.x, point.y);
    }
    glEnd();
}

class MathApplication final : public Engine::Application
{
private:
    Engine::Transform2 object = { { 400, 300 }, 0, { 1, 1 }, { 80, 50 } };

    bool onInput(const Engine::FrameInfo&, Platform::Error&) override
    {
        if (Platform::isKeyPressed(Engine::Key::Escape)) {
            requestExit();
        }
        if (Platform::isKeyPressed(Engine::Key::R)) {
            object = { { 400, 300 }, 0, { 1, 1 }, { 80, 50 } };
        }
        if (Platform::isKeyPressed(Engine::Key::X)) {
            object.scale.x = -object.scale.x;
        }
        const float zoom = static_cast<float>(Platform::getInput().mouse().wheelY) * 0.1F;
        const float magnitude = std::clamp(std::abs(object.scale.x) + zoom, 0.25F, 3.0F);
        object.scale = { std::copysign(magnitude, object.scale.x), magnitude };
        return true;
    }

    bool onUpdate(const Engine::FrameInfo& frame, Platform::Error&) override
    {
        Engine::Vector2 direction;
        if (Platform::isKeyDown(Engine::Key::ArrowLeft)) {
            direction.x -= 1;
        }
        if (Platform::isKeyDown(Engine::Key::ArrowRight)) {
            direction.x += 1;
        }
        if (Platform::isKeyDown(Engine::Key::ArrowUp)) {
            direction.y -= 1;
        }
        if (Platform::isKeyDown(Engine::Key::ArrowDown)) {
            direction.y += 1;
        }
        object.position += Engine::normalized(direction) * MovementSpeed * frame.deltaTime;
        if (Platform::isKeyDown(Engine::Key::Q)) {
            object.rotation -= RotationSpeed * frame.deltaTime;
        }
        if (Platform::isKeyDown(Engine::Key::E)) {
            object.rotation += RotationSpeed * frame.deltaTime;
        }
        return true;
    }

    bool onRender(const Engine::FrameInfo& frame, Platform::Error& error) override
    {
        const Engine::Affine2 forward = object.toAffine();
        Engine::Affine2 inverse;
        Engine::TransformError transformError;
        if (!forward.inverse(inverse, transformError)) {
            error = { "Affine2::inverse", "Object transform is not invertible.", {} };
            return false;
        }
        const auto& mouse = Platform::getInput().mouse();
        const Engine::Vector2 cursor = { static_cast<float>(mouse.x), static_cast<float>(mouse.y) };
        // В демо масштаб равномерный по модулю: обратное преобразование сохраняет форму круга.
        const Engine::Circle localCursorCircle = {
            inverse.transformPoint(cursor), CursorRadius / std::abs(object.scale.x)
        };
        const bool inside = mouse.inside && Engine::contains(LocalBounds, localCursorCircle.center);
        const Engine::Rect bounds = forward.transformBounds(LocalBounds);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, frame.window.width, frame.window.height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glClearColor(0.03F, 0.04F, 0.07F, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        setColor(inside ? HitColor : IdleColor);
        drawRectangle(LocalBounds, forward, GL_QUADS);
        setColor(DirectionColor);
        drawRectangle(DirectionShaft, forward, GL_QUADS);
        glBegin(GL_TRIANGLES);
        for (Engine::Vector2 vertex : DirectionHead) {
            const Engine::Vector2 point = forward.transformPoint(vertex);
            glVertex2f(point.x, point.y);
        }
        glEnd();
        setColor(BoundsColor);
        drawRectangle(bounds, {}, GL_LINE_LOOP);

        if (mouse.inside) {
            Engine::Color cursorColor = CursorColor;
            if (Engine::intersects(bounds, Engine::Circle{ cursor, CursorRadius })) {
                cursorColor = CursorBoundsColor;
            }
            if (Engine::intersects(LocalBounds, localCursorCircle)) {
                cursorColor = CursorHitColor;
            }
            setColor(cursorColor);
            glBegin(GL_LINE_LOOP);
            for (int index = 0; index < CircleSegments; ++index) {
                const float angle = 2 * Engine::Pi * static_cast<float>(index) / CircleSegments;
                const Engine::Vector2 point = cursor + Engine::rotated({ CursorRadius, 0 }, angle);
                glVertex2f(point.x, point.y);
            }
            glEnd();
        }
        return true;
    }
};
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#else
int main()
#endif
{
    Engine::ApplicationOptions options;
    options.window.title = "Math | arrows: move | Q/E: rotate | wheel: scale | X: reflect | R: reset | hover: local hit test";
    MathApplication application;
    std::vector<Platform::Error> errors;
    const bool success = application.run(options, errors);
    bool showDialog = true;
    for (const auto& error : errors) {
        Platform::reportError(error, showDialog);
        showDialog = false;
    }
    return success ? 0 : 1;
}
