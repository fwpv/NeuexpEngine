#pragma once

#include "camera.h"
#include "object.h"

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace NeuexpEngine
{
class Renderer2D;

// Владелец игровых объектов и единого порядка их жизни: объекты обновляются
// и рисуются в порядке добавления. Камера сцены рассчитывает вид её отрисовки.
class Scene
{
public:
    Scene() = default;
    ~Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Возвращает идентификатор нового объекта; nullptr отклоняется std::invalid_argument.
    // Объект сразу доступен запросам и отрисовке, а обновляется со следующего update(),
    // если добавлен во время обновления.
    ObjectId add(std::unique_ptr<GameObject> object);
    // Удаление отложено: объект сразу перестаёт обновляться, рисоваться и находиться
    // запросами, а освобождается в конце ближайшего update() или в clear().
    // Неизвестный и уже удалённый идентификатор игнорируются.
    void destroy(ObjectId id);

    GameObject* find(ObjectId id);
    const GameObject* find(ObjectId id) const;
    // Первый объект с точным совпадением имени; nullptr — не найден.
    GameObject* findByName(std::string_view name);
    const GameObject* findByName(std::string_view name) const;
    // Все объекты группы в порядке добавления.
    std::vector<GameObject*> findByGroup(std::string_view group);
    std::vector<const GameObject*> findByGroup(std::string_view group) const;
    // Число живых объектов; ожидающие освобождения не учитываются.
    std::size_t count() const;

    // Обновляет живые объекты в порядке добавления, затем освобождает удалённые.
    void update(float deltaTime);
    // Рисует видимые объекты в порядке добавления; вид мира задаёт приложение.
    void draw(Renderer2D& renderer) const;

    // Камера сцены; createView рассчитывает по ней вид всей клиентской области.
    Camera2& camera() { return viewCamera; }
    const Camera2& camera() const { return viewCamera; }
    bool createView(int width, int height, CameraView& result, CameraError& error) const;

    // Немедленно удаляет все объекты, включая ожидающие освобождения. Вызов из update()
    // безопасен: объекты сразу исчезают из запросов и отрисовки, а память освобождается
    // в конце текущего прохода. Идентификаторы не переиспользуются: новые объекты
    // получают бо́льшие значения.
    void clear();

private:
    void releaseRemoved();

    std::vector<std::unique_ptr<GameObject>> objects;
    ObjectId nextObjectId = 1;
    Camera2 viewCamera;
    bool updating = false;
};
}
