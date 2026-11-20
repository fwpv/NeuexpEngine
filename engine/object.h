#pragma once

#include "math2d.h"
#include "sprite.h"
#include "transform.h"

#include <cstdint>
#include <string>
#include <utility>

namespace NeuexpEngine
{
class Renderer2D;

// Уникален внутри сцены и не переиспользуется, в том числе после clear().
// Ноль означает, что объект не принадлежит сцене.
using ObjectId = std::uint64_t;

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
    bool visible = true;
    // Удалён из сцены: до освобождения памяти объект не обновляется, не рисуется
    // и не находится запросами.
    bool removalPending = false;
};
}
