#pragma once

#include "core/math2d.h"
#include "graphics/sprite.h"
#include "core/transform.h"
#include "collision/collider.h"
#include "object_id.h"
#include "physics/body.h"

#include <string>
#include <utility>

namespace NeuexpEngine
{
class Renderer2D;

// Игровой объект: трансформация, визуальное представление и необязательное поведение.
// Сцена владеет объектом, поэтому копирование и перемещение не поддерживаются:
// идентификатор и адрес объекта постоянны, пока он принадлежит сцене.
class GameObject
{
public:
    GameObject() = default;
    virtual ~GameObject() = default;
    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;
    GameObject(GameObject&&) = delete;
    GameObject& operator=(GameObject&&) = delete;

    ObjectId id() const { return objectId; }
    // Имя и группа используются поиском сцены; строки сравниваются точно.
    const std::string& name() const { return objectName; }
    void setName(std::string value) { objectName = std::move(value); }
    const std::string& group() const { return objectGroup; }
    void setGroup(std::string value) { objectGroup = std::move(value); }

    // Скрытый объект продолжает обновляться и находится запросами, но не рисуется.
    bool isVisible() const { return visible; }
    void setVisible(bool value) { visible = value; }

    // Преобразование локальных координат объекта в координаты мира.
    const Transform2& transform() const { return placement; }
    Transform2& transform() { return placement; }

    // Общее визуальное представление: базовый draw() рисует спрайт с transform объекта.
    // Текстура спрайта — общая ссылка, поэтому удаление объекта не освобождает ресурс,
    // пока им пользуются другие объекты или код приложения. Спрайт без текстуры
    // не рисуется, а спрайт не хранит анимацию: анимируемые объекты рисуют свой кадр сами.
    const Sprite& visual() const { return appearance; }
    Sprite& visual() { return appearance; }

    // Форма и правила участия в столкновениях; проверки выполняет физика сцены
    // или приложение через запросы collision.
    const Collider& collider() const { return collision; }
    Collider& collider() { return collision; }

    // Физика сцены: форму, слои, сенсор и поверхностные свойства задаёт коллайдер,
    // позу — transform (её позиция считается центром масс тела), динамику — тело.
    // Объект без включённого тела остаётся неподвижной геометрией; enablePhysics
    // копирует настройки тела и будит его, disablePhysics возвращает неподвижное тело.
    void enablePhysics(const PhysicsBody& settings = PhysicsBody{});
    void disablePhysics();
    const PhysicsBody& body() const { return physicsBody; }
    PhysicsBody& body() { return physicsBody; }

    // Импульсы меняют скорость тела сразу, силы и момент накапливаются до ближайшего
    // шага физики. Действуют только на динамическое тело; масса и инерция считаются
    // по мировому коллайдеру, мировая точка приложения даёт угловую скорость
    // от смещения относительно transform.position.
    void applyImpulse(Vector2 impulse);
    void applyImpulseAtPoint(Vector2 impulse, Vector2 worldPoint);
    void applyAngularImpulse(float impulse);
    void applyForce(Vector2 force);
    void applyForceAtPoint(Vector2 force, Vector2 worldPoint);
    void applyTorque(float torque);

    // Вызываются сценой. Обновление по умолчанию ничего не делает; базовый draw()
    // рисует visual и пропускается для спрайта без текстуры. Подкласс расширяет
    // поведение переопределением методов и при необходимости вызывает базовую отрисовку.
    virtual void update(float deltaTime);
    virtual void draw(Renderer2D& renderer) const;

private:
    friend class Scene;

    ObjectId objectId = 0;
    std::string objectName;
    std::string objectGroup;
    Transform2 placement;
    Sprite appearance;
    Collider collision;
    PhysicsBody physicsBody;
    bool visible = true;
    // Удалён из сцены; память освобождается в конце update() или в clear().
    bool removalPending = false;
};
}
