#pragma once

#include "core/math2d.h"

#include <array>
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
    std::array<Interval, 2> segments; // Локальные опорные отрезки слева направо.
    float overhang = 0; // Допустимый вылет центра за край поверхности.
    float heightAllowance = 0; // Допуск высоты при переходе между опорами.
    float minimumOverlap = 0; // Перекрытие каждого отрезка своей поверхностью при опоре между ними.
};

struct Body
{
    NeuexpEngine::Vector2 position; // X — центр опоры, Y — уровень контакта; ось Y направлена вниз.
    NeuexpEngine::Vector2 velocity;
    SupportShape supportShape;
};

struct Surface
{
    int id = -1; // Уникальный неотрицательный ID.
    NeuexpEngine::Vector2 position;
    NeuexpEngine::Size2 size;
    float speed = 0; // Вертикальная скорость; положительное направление — вниз.
    float top = 0; // При ненулевой скорости требуется top < bottom.
    float bottom = 0;
};

struct MotionRequest
{
    float horizontalTarget = 0; // Целевая скорость по X, пиксели в секунду.
    float horizontalAcceleration = 0; // Неотрицательное ускорение к целевой скорости.
    float verticalAcceleration = 0; // Неотрицательное ускорение свободного падения.
};

// Одно тело и односторонние горизонтальные поверхности; расстояния — пиксели, время — секунды.
class CatPhysics
{
public:
    // Тело и контейнер должны жить дольше решателя; указатели действительны на время update().
    CatPhysics(Body& body, const std::vector<Surface*>& surfaces);
    bool supports(int surfaceId) const;
    bool hasSupport() const;
    void reset(NeuexpEngine::Vector2 position, NeuexpEngine::Vector2 velocity);
    void launch(float upwardImpulse); // Отрыв с добавлением скорости вверх, включая скорость опоры.
    void dropThrough(float downwardImpulse); // Импульс вниз и запрет приземления на 0,15 секунды.
    // Один плавающий шаг целиком, без внутренних подшагов. duration >= 0, ускорения и импульсы >= 0.
    // Платформы разворачиваются на границах, отбрасывая остаток перемещения.
    // Опора: центр над поверхностью с overhang либо два отрезка над соседними поверхностями.
    // Близкая высота допускает переход между опорами; приземление — пересечение уровня сверху вниз.
    void update(float duration, const MotionRequest& request);

private:
    bool canSupport(const Surface& surface) const;

    Body& body;
    const std::vector<Surface*>& surfaces;
    int supportId = -1;
    float dropTime = 0;
};
}
