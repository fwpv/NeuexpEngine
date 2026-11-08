#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Engine
{
enum class ResourceErrorCode
{
    None,
    InvalidPath,
    Io,
    InvalidImage,
    LimitExceeded,
    InvalidState,
    Graphics
};

struct ResourceError
{
    ResourceErrorCode code = ResourceErrorCode::None;
    std::string message;
};

struct ResourceLimits
{
    std::size_t fileBytes = 64 * 1024 * 1024;
    std::size_t imageBytes = 64 * 1024 * 1024;
    std::size_t decoderBytes = 128 * 1024 * 1024;
    int imageDimension = 8192;
};

// Плотные строки RGBA8 сверху вниз; RGB не умножен на альфа-канал.
struct Image
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

// При отказе выходные значения сохраняются; error заменяется каждым вызовом.
bool validateImage(const Image& image, ResourceError& error, const ResourceLimits& limits = {});
// Декодирование через stb_image; отдельная проверка CRC контейнера не выполняется.
// Ошибки выделения памяти стандартной библиотекой передаются как исключения.
bool decodePng(const std::vector<std::uint8_t>& bytes, Image& result,
    ResourceError& error, const ResourceLimits& limits = {});

// Абсолютный каталог исполняемого файла, независимо от cwd и расположения ассетов.
// При ошибке result не меняется.
bool executableDirectory(std::filesystem::path& result, ResourceError& error);

class ResourceFiles
{
public:
    // Пустой root выбирает assets рядом с исполняемым файлом. Явный root должен
    // быть абсолютным. После инициализации изменение рабочей директории не влияет на пути.
    bool initialize(ResourceError& error, const std::filesystem::path& root = {});
    const std::filesystem::path& root() const;
    // Имена относительные, в UTF-8; выход за пределы root отклоняется.
    bool resolve(const std::string& name, std::filesystem::path& result, ResourceError& error) const;
    bool readBinary(const std::string& name, std::vector<std::uint8_t>& result,
        ResourceError& error, std::size_t maxBytes = ResourceLimits{}.fileBytes) const;
    // Читает байты без изменения переводов строк и без преобразования кодировки.
    bool readText(const std::string& name, std::string& result,
        ResourceError& error, std::size_t maxBytes = ResourceLimits{}.fileBytes) const;
    bool loadPng(const std::string& name, Image& result,
        ResourceError& error, const ResourceLimits& limits = {}) const;

private:
    std::filesystem::path directory;
};
}
