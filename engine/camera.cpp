#include "camera.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Engine
{
Rect Viewport::bounds() const
{
    return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)};
}

Vector2 CameraView::worldToScreen(Vector2 point) const
{
    return worldToScreenTransform.transformPoint(point);
}

bool Viewport::contains(Vector2 point) const
{
    const Rect screen = bounds();
    return std::isfinite(point.x) && std::isfinite(point.y)
        && point.x >= screen.position.x && point.y >= screen.position.y
        && point.x < screen.position.x + screen.size.width
        && point.y < screen.position.y + screen.size.height;
}

bool CameraView::screenToWorld(Vector2 point, Vector2& result) const
{
    if (!viewport.contains(point)) {
        return false;
    }
    result = screenToWorldTransform.transformPoint(point);
    return true;
}

bool Camera2::createView(const Viewport& area, CameraView& result, CameraError& error) const
{
    if (area.width <= 0 || area.height <= 0
        || area.x > std::numeric_limits<int>::max() - area.width
        || area.y > std::numeric_limits<int>::max() - area.height) {
        error = CameraError::InvalidViewport;
        return false;
    }
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(zoom) || zoom <= 0
        || !std::isfinite(referenceSize.width) || !std::isfinite(referenceSize.height)
        || referenceSize.width <= 0 || referenceSize.height <= 0
        || (mode != ViewportMode::Expand && mode != ViewportMode::Fit)) {
        error = CameraError::InvalidCamera;
        return false;
    }

    CameraView candidate;
    candidate.viewport = area;
    if (mode == ViewportMode::Fit) {
        const double scale = std::min(area.width / static_cast<double>(referenceSize.width),
            area.height / static_cast<double>(referenceSize.height));
        candidate.viewport.width = std::max(1, static_cast<int>(std::floor(referenceSize.width * scale)));
        candidate.viewport.height = std::max(1, static_cast<int>(std::floor(referenceSize.height * scale)));
        candidate.viewport.x += (area.width - candidate.viewport.width) / 2;
        candidate.viewport.y += (area.height - candidate.viewport.height) / 2;
    }

    const float pixelsPerUnit = static_cast<float>(std::min(
        candidate.viewport.width / static_cast<double>(referenceSize.width),
        candidate.viewport.height / static_cast<double>(referenceSize.height)) * zoom);
    if (!std::isfinite(pixelsPerUnit) || pixelsPerUnit <= 0) {
        error = CameraError::NonInvertible;
        return false;
    }
    const Transform2 transform = {candidate.viewport.bounds().center(), 0, Vector2{pixelsPerUnit, pixelsPerUnit}, position};
    candidate.worldToScreenTransform = transform.toAffine();
    TransformError transformError;
    if (!candidate.worldToScreenTransform.inverse(candidate.screenToWorldTransform, transformError)) {
        error = CameraError::NonInvertible;
        return false;
    }
    candidate.worldBounds = candidate.screenToWorldTransform.transformBounds(candidate.viewport.bounds());
    if (!std::isfinite(candidate.worldBounds.position.x) || !std::isfinite(candidate.worldBounds.position.y)
        || !std::isfinite(candidate.worldBounds.size.width) || !std::isfinite(candidate.worldBounds.size.height)
        || candidate.worldBounds.size.width <= 0 || candidate.worldBounds.size.height <= 0) {
        error = CameraError::NonInvertible;
        return false;
    }
    result = candidate;
    error = CameraError::None;
    return true;
}
}
