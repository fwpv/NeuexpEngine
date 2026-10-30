#pragma once

#include "math2d.h"

#include <array>
#include <optional>
#include <set>
#include <vector>

namespace Cat
{
struct Interval
{
    float left = 0;
    float right = 0;
};

struct SupportShape
{
    // Локальные опорные отрезки, упорядоченные слева направо.
    std::array<Interval, 2> segments;
    float overhang = 0;
    float captureDistance = 0;
    float heightAllowance = 0;
    float minimumOverlap = 0;
    float bridgeMargin = 0;
};

struct Body
{
    Engine::Vector2 position;
    Engine::Vector2 velocity;
    SupportShape supportShape;
};

// Односторонняя горизонтальная поверхность с ограниченным вертикальным движением.
struct Surface
{
    int id = -1; // Уникальный неотрицательный ID, независимый от адреса поверхности.
    Engine::Vector2 position;
    Engine::Size2 size;
    float speed = 0; // Вертикальная скорость, положительное направление — вниз.
    float top = 0;
    float bottom = 0;
};

struct Trajectory
{
    Engine::Vector2 positionAt(float time) const;
    Engine::Vector2 velocityAt(float time) const;

    Engine::Vector2 origin;
    Engine::Vector2 velocity;
    Engine::Vector2 acceleration;
};

struct MotionRequest
{
    float horizontalTarget = 0;
    float horizontalAcceleration = 0; // Неотрицательная величина.
    float verticalAcceleration = 0; // Неотрицательная величина, ось Y направлена вниз.
};

enum class StepEventKind
{
    None,
    ContactBegin,
    ContactEnd,
    LimitReached
};

struct StepResult
{
    float duration = 0;
    StepEventKind event = StepEventKind::None;
    int surfaceId = -1;
};

// Решатель одного тела и набора односторонних поверхностей.
class CatPhysics
{
public:
    // Тело и контейнер ссылок должны существовать дольше решателя.
    // Элементы surfaces действительны и неизменны по составу между prepareStep() и advance().
    CatPhysics(Body& body, std::vector<Surface*>& surfaces);
    bool supports(int surfaceId) const;
    bool hasSupport() const;
    bool isCapturingSupport() const;
    void clearContacts();
    void detachSupport(bool excludeContacts);
    float prepareStep(float duration);
    // maximumY ограничивает свободное движение по Y; дальнейшие действия после события определяет мир.
    StepResult advance(float duration, const MotionRequest& request, std::optional<float> maximumY = std::nullopt);

    static constexpr float TimeTolerance = 1e-9F;

private:
    struct SupportQuery
    {
        float speed = 0;
        float time = 0;
        bool allowCapture = true;
    };

    struct VelocityLimit
    {
        float value;
        float time;
    };

    struct MotionStep
    {
        Trajectory trajectory;
        VelocityLimit velocityLimit;
        float duration;
    };

    struct StepEvent
    {
        float time;
        StepEventKind kind = StepEventKind::None;
        const Surface* surface = nullptr;
        float edgeX = 0;
    };

    struct SupportState
    {
        int surfaceId = -1;
        int entrySide = 0; // 0 — устойчивая опора, +1 — захват слева, -1 — справа.
        std::set<int> excluded;
    };

    std::vector<Interval> supportIntervals(const Surface& surface, SupportQuery query) const;
    std::optional<Interval> supportInterval(const Surface& surface, float x, SupportQuery query) const;
    std::optional<Interval> nextSupportInterval(const Surface& surface, float x, SupportQuery query) const;
    const Surface* findSupport(int direction = 0, int excluded = -1) const;
    void retireExcludedSurfaces();
    void refreshSupport();
    void excludeContactSurfaces();
    void leaveEdge(const Surface& surface, float x);
    MotionStep planMotion(float duration, const MotionRequest& request) const;
    void limitSupportedMotion(const Surface& standing, bool entering, const Trajectory& trajectory, StepEvent& event) const;
    void findAirborneEvent(const Trajectory& trajectory, StepEvent& event, std::optional<float> maximumY) const;
    static float prepareSurfaceMotion(Surface& surface, float duration);

    Body& body;
    std::vector<Surface*>& surfaces;
    SupportState contactState;
};

}
