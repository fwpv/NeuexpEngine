#pragma once

#include <filesystem>
#include <string>

namespace GuiDemo
{
constexpr char SettingsFileName[] = "gui_demo_settings.tmp";

// Допустимый диапазон масштаба интерфейса:
// используется при чтении настроек и в меню.
constexpr float MinScale = 0.75F;
constexpr float MaxScale = 2.0F;

struct Settings
{
    float scale = 1;
    bool vsync = false;
    bool images = false;
    bool bitmapText = false;
    bool textFiltering = false;
};

// Отсутствующий файл оставляет начальные настройки;
// повреждённый не меняет результат.
bool loadSettings(const std::filesystem::path& path, Settings& settings, std::string& error);
bool saveSettings(const std::filesystem::path& path, const Settings& settings, std::string& error);
}
