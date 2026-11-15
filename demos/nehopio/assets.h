#pragma once

#include "animation.h"
#include "atlas.h"
#include "font.h"
#include "resources.h"
#include "texture.h"

#include <array>
#include <map>

namespace Engine
{
class Renderer2D;
}

namespace Nehopio
{
// Ресурсы уровня: картинки, листы с кадрами и подготовленные кадры объектов.
// Всё визуальное готовится здесь один раз, а игровые классы получают копии кадров.
struct Assets
{
    bool load(Engine::Renderer2D& renderer, const Engine::ResourceFiles& files, Engine::ResourceError& error);

    // Растягиваемые заливки: небо, провал и панели интерфейса.
    Engine::Texture sky;
    Engine::Texture abyss;
    Engine::Texture panel;
    // Листы с кадрами: участки заданы описаниями атласов.
    Engine::TextureAtlas playerAtlas;
    Engine::TextureAtlas playerBigAtlas;
    Engine::TextureAtlas enemyAtlas;
    Engine::TextureAtlas flyerAtlas;
    Engine::TextureAtlas coinAtlas;
    Engine::TextureAtlas tileAtlas;
    Engine::TextureAtlas cloudAtlas;

    // Шрифт интерфейса: изображение-маска глифов и описание раскладки.
    Engine::BitmapFont font;

    // Кадры объектов уровня: каждый объект получает их копию при перезапуске.
    Engine::Animation coin;
    Engine::Animation popup;
    Engine::AnimationSet walker;
    Engine::AnimationSet flyer;
    Engine::Sprite mushroomSprite;
    // Кадры персонажа: обычная и увеличенная формы.
    std::array<Engine::AnimationSet, 2> player;

    // Спрайты уровня: флаг, холм, куст, плитки по символу карты и облака.
    Engine::Sprite flagSprite;
    Engine::Sprite hillSprite;
    Engine::Sprite bushSprite;
    std::map<char, Engine::Sprite> tileSprites;
    std::array<Engine::Sprite, 2> cloudSprites;
};
}
