#include "atlas.h"
#include "geometry.h"
#include "resources.h"
#include "sprite.h"
#include "texture.h"

#include <cstddef>
#include <functional>
#include <locale>
#include <map>
#include <sstream>
#include <string>
#include <utility>

namespace Engine
{
bool TextureAtlas::load(const Texture& source, const std::string& description, ResourceError& error)
{
    std::map<std::string, Rect, std::less<>> parsed;
    std::istringstream lines(description);
    std::string line;
    std::size_t number = 0;
    if (!source.isValid()) {
        error = {ResourceErrorCode::InvalidState, "Atlas requires a valid texture."};
        return false;
    }
    while (std::getline(lines, line)) {
        ++number;
        line = line.substr(0, line.find('#'));
        std::istringstream fields(line);
        fields.imbue(std::locale::classic());
        std::string name;
        if (!(fields >> name)) {
            continue;
        }
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        std::string extra;
        if (!(fields >> x >> y >> width >> height) || (fields >> extra)
            || x < 0 || y < 0 || width <= 0 || height <= 0
            || width > source.width() || height > source.height()
            || x > source.width() - width || y > source.height() - height
            || !parsed.emplace(name, Rect{static_cast<float>(x), static_cast<float>(y),
                static_cast<float>(width), static_cast<float>(height)}).second) {
            error = {ResourceErrorCode::InvalidImage, "Invalid atlas region at line " + std::to_string(number)};
            return false;
        }
    }
    if (parsed.empty()) {
        error = {ResourceErrorCode::InvalidImage, "Atlas contains no regions."};
        return false;
    }
    storedTexture = source;
    regions = std::move(parsed);
    error = {};
    return true;
}

bool TextureAtlas::load(const Texture& source, const ResourceFiles& files, const std::string& name, ResourceError& error)
{
    std::string description;
    return files.readText(name, description, error) && load(source, description, error);
}

const Rect* TextureAtlas::find(std::string_view name) const
{
    const auto found = regions.find(name);
    return found == regions.end() ? nullptr : &found->second;
}

bool TextureAtlas::createSprite(std::string_view name, Sprite& result) const
{
    const Rect* area = find(name);
    if (!area || !storedTexture.isValid()) {
        return false;
    }
    result = Sprite(storedTexture, *area);
    return true;
}
}
