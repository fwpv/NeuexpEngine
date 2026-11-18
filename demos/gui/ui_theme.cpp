#include "ui_theme.h"

#include <cstddef>
#include <string>

namespace GuiDemo
{
namespace
{
constexpr std::array<const char*, 3> SkinStates{"normal", "hover", "pressed"};
constexpr NE::Size2 WideButtonFallbackSize{150, 41};
constexpr NE::Size2 NarrowButtonFallbackSize{96, 41};
constexpr NE::Color TextColor{0.93F, 0.96F, 1, 1};
constexpr NE::Color ButtonTextColor{0.07F, 0.12F, 0.2F, 1};
constexpr float NarrowButtonWidth = 100;
constexpr float ButtonWidthFactor = 0.89F;

// Копирует три состояния скина в Normal/Hover/Pressed;
// Disabled остаётся пустым.
void applyImages(std::array<NE::Sprite, 4>& target, const std::array<NE::Sprite, 3>& source)
{
    target = {};
    for (std::size_t index = 0; index < source.size(); ++index) {
        target[index] = source[index];
    }
}
}

bool Skin::load(const NE::TextureAtlas& atlas)
{
    for (std::size_t index = 0; index < SkinStates.size(); ++index) {
        const std::string suffix = std::string("_") + SkinStates[index];
        if (!atlas.createSprite(std::string("btn1") + suffix, wideButton[index])
            || !atlas.createSprite(std::string("btn2") + suffix, narrowButton[index])
            || !atlas.createSprite(std::string("check_box_off") + suffix, checkOff[index])
            || !atlas.createSprite(std::string("check_box_on") + suffix, checkOn[index])) {
            return false;
        }
        wideButton[index].filter = NE::TextureFilter::Nearest;
        narrowButton[index].filter = NE::TextureFilter::Nearest;
    }
    return true;
}

NE::Size2 buttonSize(const UiResources& resources, float width)
{
    const bool narrow = width < NarrowButtonWidth;
    const auto region = resources.atlas.find(narrow ? "btn2_normal" : "btn1_normal");
    NE::Size2 imageSize = narrow ? NarrowButtonFallbackSize : WideButtonFallbackSize;
    if (region) {
        imageSize = region->size;
    }
    const float buttonWidth = width * ButtonWidthFactor;
    return NE::Size2{buttonWidth, buttonWidth * imageSize.height / imageSize.width};
}

void resetStyle(UI::Style& style, const Settings& settings, const NE::BitmapFont& font)
{
    if (settings.bitmapText) {
        style.font = font;
    } else {
        style.font = NE::BitmapFont{};
    }
    style.textFilter = settings.textFiltering ? NE::TextureFilter::Linear : NE::TextureFilter::Nearest;
    style.images = {};
    style.checkedImages = {};
    style.foreground = TextColor;
}

void applyStyle(const UiResources& resources, const Settings& settings, const UiElements& elements)
{
    for (const auto& element : elements.text) {
        resetStyle(element->style(), settings, resources.font);
    }
    if (!settings.images || !resources.skinReady) {
        return;
    }
    for (const auto& button : elements.buttons) {
        UI::Style& style = button->style();
        style.foreground = ButtonTextColor;
        if (button->placement().size.width < NarrowButtonWidth) {
            applyImages(style.images, resources.skin.narrowButton);
        } else {
            applyImages(style.images, resources.skin.wideButton);
        }
    }
    for (const auto& checkbox : elements.checkboxes) {
        UI::Style& style = checkbox->style();
        applyImages(style.images, resources.skin.checkOff);
        applyImages(style.checkedImages, resources.skin.checkOn);
    }
}
}
