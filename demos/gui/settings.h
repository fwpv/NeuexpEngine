#pragma once

#include <filesystem>
#include <string>

namespace GuiDemo
{
struct Settings
{
    float scale = 1;
    bool vsync = false;
    bool images = false;
    bool bitmapText = false;
    bool textFiltering = true;
};

// Отсутствующий файл оставляет начальные настройки; повреждённый не меняет результат.
bool loadSettings(const std::filesystem::path& path, Settings& settings, std::string& error);
bool saveSettings(const std::filesystem::path& path, const Settings& settings, std::string& error);
}
