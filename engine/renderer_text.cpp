#include "renderer.h"
#include "detail/font_data.h"
#include "detail/texture_data.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <filesystem>
#include <utility>

namespace Engine
{
bool Renderer2D::loadFont(const ResourceFiles& files, const std::string& name,
    BitmapFont& result, ResourceError& error)
{
    std::string description;
    Texture texture;
    if (!loadFontImage(files, name, texture, description, error)) {
        return false;
    }
    return result.load(texture, description, error);
}

bool Renderer2D::loadFontImage(const ResourceFiles& files, const std::string& name, Texture& result,
    std::string& description, ResourceError& error)
{
    if (!initialized) {
        error = {ResourceErrorCode::InvalidState, "Initialize the renderer before loading fonts."};
        return false;
    }
    std::string text;
    Detail::FontData parsed;
    if (!files.readText(name, text, error) || !Detail::parseFont(text, parsed, error)) {
        return false;
    }
    const auto imageName = (std::filesystem::u8path(name).parent_path()
        / std::filesystem::u8path(parsed.image)).generic_u8string();
    Image image;
    if (!files.loadPng(imageName, image, error)) {
        return false;
    }
    // Альфа — маска глифа. Белый RGB нужен и прозрачным текселям для Linear.
    for (std::size_t pixel = 0; pixel < image.pixels.size(); pixel += 4) {
        image.pixels[pixel] = 255;
        image.pixels[pixel + 1] = 255;
        image.pixels[pixel + 2] = 255;
    }
    if (!createTexture(image, result, error)) {
        return false;
    }
    description = std::move(text);
    return true;
}

void Renderer2D::drawText(const TextLayout& text, Vector2 position, const DrawOptions& options,
    TextureFilter filter)
{
    if (!ready() || !prepareDraw(options)) {
        return;
    }
    const Texture texture = text.texture();
    if (!text.isValid() || texture.data->owner != this) {
        firstError = RenderError::InvalidTexture;
        return;
    }
    if ((filter != TextureFilter::Nearest && filter != TextureFilter::Linear)) {
        firstError = RenderError::InvalidOptions;
        return;
    }
    DrawOptions translated = options;
    translated.transform = options.transform * Transform2{position, 0, Vector2{1, 1}, Vector2{}}.toAffine();
    if (!prepareDraw(translated) || text.glyphs().empty() || options.color.alpha == 0) {
        return;
    }
    TextureSampling sampling;
    sampling.filter = filter;
    translated.transform = viewTransform * translated.transform;
    const auto& mesh = text.vertices(filter);
    std::vector<Vector2> positions;
    positions.reserve(mesh.size());
    for (const auto& vertex : mesh) {
        positions.push_back(translated.transform.transformPoint(vertex.position));
    }
    commands.push_back(Command{Primitive::Quads, std::move(positions), translated, clip, std::nullopt,
        texture, sampling, std::nullopt, text});
}

void Renderer2D::executeText(const Command& command) const
{
    const auto& vertices = command.text->vertices(command.sampling.filter);
    const auto& color = command.options.color;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, command.texture.data->id);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    const GLint filter = command.sampling.filter == TextureFilter::Nearest ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    // Раскладка не знает размер GPU-хранилища: нормализация выполняется здесь.
    glScalef(1.0F / static_cast<float>(command.texture.data->storageWidth),
        1.0F / static_cast<float>(command.texture.data->storageHeight), 1);
    glMatrixMode(GL_MODELVIEW);
    glColor4f(color.red, color.green, color.blue, color.alpha);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_INDEX_ARRAY);
    glDisableClientState(GL_EDGE_FLAG_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, sizeof(Vector2), command.vertices.data());
    glTexCoordPointer(2, GL_FLOAT, sizeof(TextVertex), &vertices.front().texel);
    glDrawArrays(GL_QUADS, 0, static_cast<GLsizei>(vertices.size()));
    glPopClientAttrib();
    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
}

}
