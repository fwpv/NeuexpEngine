#include "settings.h"

#include <cmath>
#include <fstream>
#include <locale>
#include <system_error>

namespace GuiDemo
{
bool loadSettings(const std::filesystem::path& path, Settings& settings, std::string& error)
{
    error.clear();
    std::error_code failure;
    const bool exists = std::filesystem::exists(path, failure);
    if (failure) {
        error = failure.message();
        return false;
    }
    if (!exists) { return true; }
    std::ifstream input(path);
    input.imbue(std::locale::classic());
    std::string signature;
    Settings candidate;
    int vsync = 0;
    int images = 0;
    int bitmap = 0;
    if (!(input >> signature >> candidate.scale >> vsync >> images >> bitmap)
        || signature != "gui_settings_v1" || !std::isfinite(candidate.scale)
        || candidate.scale < 0.75F || candidate.scale > 2
        || vsync < 0 || vsync > 1 || images < 0 || images > 1 || bitmap < 0 || bitmap > 1) {
        error = "Cannot read GUI settings: invalid file or values.";
        return false;
    }
    input >> std::ws;
    if (!input.eof()) {
        error = "Unexpected data in GUI settings.";
        return false;
    }
    candidate.vsync = vsync != 0;
    candidate.images = images != 0;
    candidate.bitmapText = bitmap != 0;
    settings = candidate;
    return true;
}
bool saveSettings(const std::filesystem::path& path, const Settings& settings, std::string& error)
{
    error.clear();
    std::ofstream output(path, std::ios::trunc);
    output.imbue(std::locale::classic());
    output << "gui_settings_v1\n" << settings.scale << '\n' << settings.vsync << '\n'
        << settings.images << '\n' << settings.bitmapText << '\n';
    output.close();
    if (!output) {
        error = "Cannot write GUI settings beside the executable.";
        return false;
    }
    return true;
}
}
