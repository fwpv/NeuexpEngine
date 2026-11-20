#pragma once

#include "core/math2d.h"
#include "scene/object.h"
#include "core/resources.h"
#include "scene/scene.h"
#include "graphics/sprite.h"
#include "core/texture.h"

#include <string>

namespace SceneDemo
{
// Мир демонстрации занимает фиксированную область 800x600 единиц.
constexpr float WorldWidth = 800;
constexpr float WorldHeight = 600;

// Кадры одной процедурной текстуры: обе сцены пользуются ею совместно.
enum class Art
{
    Rock,
    Ship,
    Star,
    Box
};

// Изображение с кадрами Art в порядке перечисления.
NeuexpEngine::Image makeArtImage();
// Спрайт кадра Art, повёрнутый вокруг собственного центра.
NeuexpEngine::Sprite makeSprite(const NeuexpEngine::Texture& texture, Art art);

// Объект на орбите: вращается вокруг центра мира и удаляет себя по истечении
// времени жизни; нулевое время жизни означает, что объект не истекает.
class Orbiter : public NeuexpEngine::GameObject
{
public:
    Orbiter(NeuexpEngine::Scene& owner, float radius, float phase, float speed, float lifetime);
    void update(float deltaTime) override;

private:
    NeuexpEngine::Scene& owner;
    float radius = 0;
    float phase = 0;
    float speed = 0;
    float lifetime = 0;
    float elapsed = 0;
};

// Счётчики исчезновений для HUD: камень пропадает только после касания корабля,
// а звезда — по истечении времени жизни.
struct Statistics
{
    int caught = 0;
    int expired = 0;
};

// Охотник: подходит к ближайшему объекту группы и удаляет его при касании.
// Так объект удаляет другой объект во время обхода сцены.
class Hunter : public NeuexpEngine::GameObject
{
public:
    Hunter(NeuexpEngine::Scene& owner, std::string targetGroup, Statistics& statistics);
    void update(float deltaTime) override;

private:
    NeuexpEngine::Scene& owner;
    std::string targetGroup;
    Statistics& statistics;
};

// Плавающий объект: колеблется по вертикали и покачивается вокруг своей оси.
class Floater : public NeuexpEngine::GameObject
{
public:
    Floater(float baseY, float amplitude, float phase);
    void update(float deltaTime) override;

private:
    float baseY = 0;
    float amplitude = 0;
    float phase = 0;
    float elapsed = 0;
};

// Население орбитальной сцены: столько камней и звёзд поддерживает хранитель.
constexpr int RockTarget = 22;
constexpr int StarTarget = 9;

// Хранитель населения: создаёт замену исчезнувшему камню или звезде, поэтому
// число объектов не растёт и не падает. Замены появляются во время обновления
// сцены и начинают обновляться со следующего кадра.
class Warden : public NeuexpEngine::GameObject
{
public:
    Warden(NeuexpEngine::Scene& owner, const NeuexpEngine::Texture& texture, Statistics& statistics);
    void update(float deltaTime) override;

private:
    void spawnRock();
    void spawnStar();

    NeuexpEngine::Scene& owner;
    NeuexpEngine::Texture texture;
    Statistics& statistics;
    // Номера продолжают нумерацию исходного населения: имена остаются уникальными.
    int nextRock = RockTarget;
    int nextStar = StarTarget;
};

// Наполнение пустых сцен: камера, объекты, имена и группы. statistics обнуляет вызывающий.
void populateOrbit(NeuexpEngine::Scene& scene, const NeuexpEngine::Texture& texture, Statistics& statistics);
void populateGrid(NeuexpEngine::Scene& scene, const NeuexpEngine::Texture& texture, Statistics& statistics);
}
