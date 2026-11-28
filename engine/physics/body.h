#pragma once

#include "collision/collider.h"
#include "collision/shape.h"
#include "core/geometry.h"
#include "core/math2d.h"
#include "scene/object.h"

namespace NeuexpEngine
{
// Вид участия тела в симуляции.
enum class BodyType
{
    // Неподвижное тело: положение и угол не меняются, форма служит препятствием.
    Static,
    // Динамическое тело: движется под действием гравитации, сил и контактов.
    Dynamic,
    // Кинематическое тело: движется собственной скоростью, но не реагирует на силы
    // и контакты; динамические тела отталкиваются от него. Движение задаёт приложение.
    Kinematic
};

// Тело физического мира. Локальные координаты формы отсчитываются от точки (0, 0) тела:
// position — положение этой точки в мире, angle — её поворот в радианах. Скорости и
// размеры задаются в единицах мира, угловая скорость — в радианах в секунду.
// Масштаб и origin объекта сцены к телу не применяются: размер тела задаёт форма.
struct RigidBody
{
    RigidBody() = default;
    RigidBody(const CollisionShape& shape, BodyType type, Vector2 position = {}, float angle = 0);

    // Форма в мировых координатах; прямоугольник вдоль осей при повороте становится
    // повёрнутым прямоугольником.
    CollisionShape worldShape() const;
    Rect worldBounds() const;

    // Площадь формы и масса: плотность, умноженная на площадь. Инерция относительно оси Z
    // через position учитывает смещение центра формы параллельным переносом.
    float area() const;
    float mass() const;
    float inertia() const;
    // У неподвижного и кинематического тела обратные масса и инерция равны нулю, поэтому
    // силы и импульсы на него не действуют. Тело с нулевой массой ведёт себя так же.
    float inverseMass() const;
    float inverseInertia() const;

    // Импульс меняет скорость сразу; на неподвижное и кинематическое тело не действует.
    void applyImpulse(Vector2 impulse);
    // worldPoint — мировая точка приложения: смещение от position даёт угловую скорость.
    void applyImpulseAtPoint(Vector2 impulse, Vector2 worldPoint);
    void applyAngularImpulse(float impulse);

    // Силы и момент накапливаются до ближайшего шага мира, после шага сбрасываются.
    // На неподвижное и кинематическое тело не действуют.
    void applyForce(Vector2 force);
    void applyForceAtPoint(Vector2 force, Vector2 worldPoint);
    void applyTorque(float torque);

    CollisionShape shape;
    BodyType type = BodyType::Static;
    Vector2 position;
    float angle = 0;
    Vector2 velocity;
    float angularVelocity = 0;
    float density = 1;
    // Коэффициенты пары сочетаются: трение — средним геометрическим, упругость — большей.
    float friction = 0.5F;
    // Доля возвращённой скорости при ударе, от 0 до 1.
    float restitution = 0;
    // Скорость умножается на 1 / (1 + затухание * dT); неотрицательные значения.
    float linearDamping = 0;
    float angularDamping = 0;
    // Доля гравитации мира, действующая на тело.
    float gravityScale = 1;
    // Сенсор пропускает тела и сообщает только о контакте.
    bool sensor = false;
    // Слои и маски пар, как у коллайдера сцены.
    CollisionLayer layer = 1U;
    CollisionLayer mask = AllCollisionLayers;
    // Объект сцены, в трансформацию которого пишется положение тела при sync(); 0 — нет.
    ObjectId object = 0;

    // Накопленные за текущий шаг сила и момент; мир сбрасывает их после шага.
    Vector2 force;
    float torque = 0;

    // Тело с малой скоростью засыпает: мир перестаёт его интегрировать и обнуляет
    // скорость, пока тело не разбудит контакт с движущимся телом, внешняя сила или
    // заданная скорость. Время неподвижности ведёт мир; изменять его приложению не нужно.
    bool sleeping = false;
    float sleepTime = 0;

    // Будит тело: нужно после ручного переноса или изменения положения спящего тела,
    // иначе мир не заметит изменения и тело останется на месте.
    void wake() { sleeping = false; sleepTime = 0; }
};
}
