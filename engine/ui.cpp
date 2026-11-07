#include "ui.h"
#include "camera.h"
#include "math2d.h"

#include <cmath>
#include <limits>

namespace Engine
{
Vector2 UiView::uiToScreen(Vector2 point) const
{
    return uiToScreenTransform.transformPoint(point);
}

bool UiView::screenToUi(Vector2 point, Vector2& result) const
{
    if (!viewport.contains(point)) {
        return false;
    }
    result = screenToUiTransform.transformPoint(point);
    return true;
}

bool UiCanvas::createView(const Viewport& area, UiView& result, UiError& error) const
{
    if (area.width <= 0 || area.height <= 0
        || area.x > std::numeric_limits<int>::max() - area.width
        || area.y > std::numeric_limits<int>::max() - area.height) {
        error = UiError::InvalidViewport;
        return false;
    }
    if (scale <= 0) {
        error = UiError::InvalidScale;
        return false;
    }
    UiView candidate;
    candidate.viewport = area;
    candidate.size = {static_cast<float>(area.width) / scale, static_cast<float>(area.height) / scale};
    candidate.uiToScreenTransform = {scale, 0, static_cast<float>(area.x), 0, scale, static_cast<float>(area.y)};
    if (!std::isfinite(candidate.size.width) || !std::isfinite(candidate.size.height)
        || candidate.size.width <= 0 || candidate.size.height <= 0
        || !candidate.uiToScreenTransform.inverse(candidate.screenToUiTransform)) {
        error = UiError::NonInvertible;
        return false;
    }
    result = candidate;
    error = UiError::None;
    return true;
}
}
