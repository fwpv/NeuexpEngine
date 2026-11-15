#pragma once

#include "camera.h"
#include "math2d.h"
#include "transform.h"

namespace NeuexpEngine
{
enum class UiError
{
    None,
    InvalidViewport,
    InvalidScale,
    NonInvertible
};

// Снимок интерфейса: начало сверху слева, +y вниз; size задан в логических единицах UI.
struct UiView
{
    Vector2 uiToScreen(Vector2 point) const;
    // point — фактические клиентские пиксели. Вне viewport result не меняется.
    // Правая и нижняя границы исключены; для захвата мыши доступна screenToUiTransform.
    bool screenToUi(Vector2 point, Vector2& result) const;

    Viewport viewport;
    Size2 size;
    Affine2 uiToScreenTransform;
    Affine2 screenToUiTransform;
};

struct UiCanvas
{
    // Конечность scale — предусловие; положительность scale и размеров area проверяется.
    // result не меняется при отказе; error заменяется при каждом вызове.
    bool createView(const Viewport& area, UiView& result, UiError& error) const;

    // Фактические пиксели на единицу UI; не зависит от Camera2::zoom.
    // Приложение может один раз взять начальное значение из WindowState::systemScale.
    // Изменения DPI и окна сами по себе не изменяют выбранный масштаб.
    float scale = 1;
};
}
