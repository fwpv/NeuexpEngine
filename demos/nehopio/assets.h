#pragma once

#include "graphics/animation.h"
#include "graphics/atlas.h"
#include "graphics/font.h"
#include "core/resources.h"
#include "core/texture.h"

#include <array>
#include <map>

namespace NeuexpEngine
{
class Renderer2D;
}

namespace Nehopio
{
// Ресурсы уровня: картинки, листы с кадрами и подготовленные кадры объектов.
// Всё визуальное готовится здесь один раз, а игровые классы получают копии кадров.
struct Assets
{
    bool load(NeuexpEngine::Renderer2D& renderer, const NeuexpEngine::ResourceFiles& files, NeuexpEngine::ResourceError& error);

    // Растягиваемые заливки: небо, провал и панели интерфейса.
    NeuexpEngine::Texture sky;
    NeuexpEngine::Texture abyss;
    NeuexpEngine::Texture panel;
    // Листы с кадрами: участки заданы описаниями атласов.
    NeuexpEngine::TextureAtlas playerAtlas;
    NeuexpEngine::TextureAtlas playerBigAtlas;
    NeuexpEngine::TextureAtlas enemyAtlas;
    NeuexpEngine::TextureAtlas flyerAtlas;
    NeuexpEngine::TextureAtlas coinAtlas;
    NeuexpEngine::TextureAtlas tileAtlas;
    NeuexpEngine::TextureAtlas cloudAtlas;

    // Шрифт интерфейса: изображение-маска глифов и описание раскладки.
    NeuexpEngine::BitmapFont font;

    // Кадры объектов уровня: каждый объект получает их копию при перезапуске.
    NeuexpEngine::Animation coin;
    NeuexpEngine::Animation popup;
    NeuexpEngine::AnimationSet walker;
    NeuexpEngine::AnimationSet flyer;
    NeuexpEngine::Sprite mushroomSprite;
    // Кадры персонажа: обычная и увеличенная формы.
    std::array<NeuexpEngine::AnimationSet, 2> player;

    // Спрайты уровня: флаг, холм, куст, плитки по символу карты и облака.
    NeuexpEngine::Sprite flagSprite;
    NeuexpEngine::Sprite hillSprite;
    NeuexpEngine::Sprite bushSprite;
    std::map<char, NeuexpEngine::Sprite> tileSprites;
    std::array<NeuexpEngine::Sprite, 2> cloudSprites;
};
}
