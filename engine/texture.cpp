#include "texture.h"
#include "detail/texture_data.h"

namespace Engine
{
bool Texture::isValid() const { return data && data->id != 0; }
int Texture::width() const { return isValid() ? data->width : 0; }
int Texture::height() const { return isValid() ? data->height : 0; }
const Image* Texture::image() const { return isValid() ? data->image.get() : nullptr; }
bool Texture::operator==(const Texture& other) const { return data == other.data; }
bool Texture::operator!=(const Texture& other) const { return !(*this == other); }
}
