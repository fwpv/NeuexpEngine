#pragma once

#include "collision/shape.h"
#include "core/math2d.h"

namespace NeuexpEngine
{
class GameObject;

// Вид участия тела в симуляции.
enum class BodyType
{
    // Неподвижное тело: положение и угол не меняются, форма служит препятствием.
    Static,
    // Динамическое тело: движется под действием гравитации, сил и контактов.
    Dynamic,
    // Кинематическое тело: движется собственной скоростью, но не реагирует на силы
    // и контакты; динамические тела отталкиваются от него. Движение задаёт приложение,
    // меняя скорость или трансформацию объекта между шагами.
    Kinematic
};

// Состояние тела объекта сцены: динамика и материалы. Форму, слои, маску и сенсор
// задаёт коллайдер объекта, позу — его transform: мир физики читает трансформацию
// в начале шага и записывает её обратно, поэтому геометрия объекта всегда актуальна.
struct PhysicsBody
{
    // Будит тело: нужно после ручного переноса, изменения позы или смены коллайдера,
    // иначе мир может не заметить изменение.
    void wake()
    {
        sleeping = false;
        sleepTime = 0;
    }

    BodyType type = BodyType::Static;
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

    // Накопленные за текущий шаг сила и момент; мир сбрасывает их после шага.
    Vector2 force;
    float torque = 0;

    // Тело с малой скоростью засыпает: мир перестаёт его интегрировать и обнуляет
    // скорость, пока тело не разбудит контакт с движущимся телом, внешняя сила,
    // заданная скорость или грань трансформации. Время неподвижности ведёт мир.
    bool sleeping = false;
    float sleepTime = 0;
};

// Масса и инерция формы в мировых координатах относительно pivot; масштаб и origin
// уже учтены в форме. Плотность умножается на площадь формы.
struct BodyMass
{
    float mass = 0;
    // Инерция относительно pivot, а не только центра формы.
    float inertia = 0;
};

BodyMass bodyMassData(const CollisionShape& shape, Vector2 pivot, float density);

// Масса и инерция тела относительно transform.position. Форма берётся из мирового
// коллайдера, поэтому учитывает масштаб и origin объекта; плотность задаёт тело.
float bodyMass(const GameObject& object);
float bodyInertia(const GameObject& object);

// Импульсы меняют скорость сразу, силы и момент накапливаются до ближайшего шага
// мира. На неподвижное и кинематическое тело не действуют. Мировая точка приложения
// даёт угловую скорость от смещения относительно transform.position.
void applyImpulse(GameObject& object, Vector2 impulse);
void applyImpulseAtPoint(GameObject& object, Vector2 impulse, Vector2 worldPoint);
void applyAngularImpulse(GameObject& object, float impulse);
void applyForce(GameObject& object, Vector2 force);
void applyForceAtPoint(GameObject& object, Vector2 force, Vector2 worldPoint);
void applyTorque(GameObject& object, float torque);
}
