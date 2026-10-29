#pragma once

#include "geometry.h"
#include "transform.h"

namespace Engine
{
// Область клиентского окна в пикселях: начало сверху слева, +y вниз.
struct Viewport
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    Rect bounds() const;
    // Клиентские пиксели; правая и нижняя границы исключены.
    bool contains(Vector2 point) const;
};

enum class ViewportMode
{
    // Сохраняет referenceSize / zoom целиком, расширяя обзор по одной из осей.
    Expand,
    // Вписывает опорные пропорции в область окна с полями.
    Fit
};

enum class CameraError
{
    None,
    InvalidViewport,
    InvalidCamera,
    NonInvertible
};

// Снимок, рассчитанный createView(). Обе системы координат имеют +x вправо и +y вниз.
// viewport учитывает поля; матрицы переводят координаты относительно всего клиентского окна.
struct CameraView
{
    Viewport viewport;
    Rect worldBounds;
    Affine2 worldToScreenTransform;
    Affine2 screenToWorldTransform;

    Vector2 worldToScreen(Vector2 point) const;
    // При точке за viewport, в том числе в полях, возвращает false и сохраняет result.
    // Правая и нижняя границы исключены. point — клиентские пиксели мыши.
    // Для преобразования без проверки границ доступна screenToWorldTransform.
    bool screenToWorld(Vector2 point, Vector2& result) const;
};

struct Camera2
{
    // Центр видимой области в мировых единицах.
    Vector2 position;
    // Положительный масштаб: увеличение приближает мир; масштаб интерфейса независим.
    float zoom = 1;
    Size2 referenceSize = { 800, 600 };
    ViewportMode mode = ViewportMode::Expand;

    // Пересчитывается после изменения камеры или размеров окна; не обращается к ОС/OpenGL.
    // Все числовые настройки конечные, referenceSize положительный. area имеет положительные размеры.
    // Fit округляет размер viewport до целых пикселей; погрешность пропорций компенсируется обзором.
    // result не меняется при отказе; error заменяется при каждом вызове.
    bool createView(const Viewport& area, CameraView& result, CameraError& error) const;
};
}
