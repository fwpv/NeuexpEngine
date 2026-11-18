#pragma once

#include "atlas.h"
#include "font.h"
#include "gui/element.h"
#include "math2d.h"
#include "settings.h"
#include "sprite.h"

#include <array>
#include <memory>
#include <vector>

namespace GuiDemo
{
namespace NE = NeuexpEngine;
namespace UI = NE::Gui;

// Спрайты скина: широкая и узкая кнопки,
// выключенный и включённый флажок.
struct Skin
{
    bool load(const NE::TextureAtlas& atlas);

    std::array<NE::Sprite, 3> wideButton;
    std::array<NE::Sprite, 3> narrowButton;
    std::array<NE::Sprite, 3> checkOff;
    std::array<NE::Sprite, 3> checkOn;
};

// Общие ресурсы интерфейса:
// атлас скина, готовые спрайты и растровый шрифт.
struct UiResources
{
    NE::TextureAtlas atlas;
    Skin skin;
    NE::BitmapFont font;
    bool skinReady = false;
};

// Элементы меню или HUD, которым применяется общий стиль.
struct UiElements
{
    std::vector<std::shared_ptr<UI::Element>> text;
    std::vector<std::shared_ptr<UI::Button>> buttons;
    std::vector<std::shared_ptr<UI::Checkbox>> checkboxes;
};

// Размер кнопки по запрошенной ширине
// и пропорциям выбранного скина.
NE::Size2 buttonSize(const UiResources& resources, float width);

// Базовое оформление элемента;
// спрайты скина добавляются отдельно.
void resetStyle(UI::Style& style, const Settings& settings, const NE::BitmapFont& font);

// Применяет тему к элементам:
// базовый стиль, затем скин кнопок и флажков.
void applyStyle(const UiResources& resources, const Settings& settings, const UiElements& elements);
}
