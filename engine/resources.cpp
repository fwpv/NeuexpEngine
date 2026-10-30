#include "resources.h"
#include "detail/executable_path.h"

#include <fstream>
#include <limits>
#include <new>
#include <utility>

namespace Engine
{
bool ResourceFiles::initialize(ResourceError& error, const std::filesystem::path& root)
{
    std::filesystem::path candidate = root;
    if (candidate.empty()) {
        if (!Detail::executableDirectory(candidate, error)) {
            return false;
        }
        candidate /= "assets";
    }
    if (!candidate.is_absolute()) {
        error = {ResourceErrorCode::InvalidPath, "The resource directory must be absolute."};
        return false;
    }
    std::error_code code;
    const auto requested = candidate;
    candidate = std::filesystem::canonical(candidate, code);
    if (code || !std::filesystem::is_directory(candidate, code) || code) {
        error = {ResourceErrorCode::Io, "The resource directory is unavailable: " + requested.u8string()};
        return false;
    }
    directory = std::move(candidate);
    error = {};
    return true;
}

const std::filesystem::path& ResourceFiles::root() const
{
    return directory;
}

bool ResourceFiles::resolve(const std::string& name, std::filesystem::path& result, ResourceError& error) const
{
    if (directory.empty()) {
        error = {ResourceErrorCode::InvalidState, "The resource directory is not initialized."};
        return false;
    }
    if (name.empty() || name.find('\0') != std::string::npos) {
        error = {ResourceErrorCode::InvalidPath, "Empty resource name or embedded null character."};
        return false;
    }
    std::filesystem::path relative;
    try {
        relative = std::filesystem::u8path(name);
    } catch (const std::filesystem::filesystem_error&) {
        error = {ResourceErrorCode::InvalidPath, "Cannot convert the UTF-8 resource name."};
        return false;
    }
    if (relative.has_root_path()) {
        error = {ResourceErrorCode::InvalidPath, "Resource names must be relative: " + name};
        return false;
    }
    std::error_code code;
    const auto candidate = std::filesystem::weakly_canonical(directory / relative, code);
    if (code) {
        error = {ResourceErrorCode::Io, "Cannot resolve resource: " + name + ": " + code.message()};
        return false;
    }
    const auto local = candidate.lexically_relative(directory);
    if (local.empty() || *local.begin() == ".." || local == ".") {
        error = {ResourceErrorCode::InvalidPath, "Resource is outside its directory: " + name};
        return false;
    }
    result = candidate;
    error = {};
    return true;
}

bool ResourceFiles::readBinary(const std::string& name, std::vector<std::uint8_t>& result,
    ResourceError& error, std::size_t maxBytes) const
{
    std::filesystem::path path;
    if (!resolve(name, path, error)) {
        return false;
    }
    std::error_code code;
    if (!std::filesystem::is_regular_file(path, code) || code) {
        error = {ResourceErrorCode::Io, "Resource is not a readable regular file: " + name};
        return false;
    }
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    const auto length = input.tellg();
    if (!input || length < 0) {
        error = {ResourceErrorCode::Io, "Cannot open resource: " + name};
        return false;
    }
    if (static_cast<std::uintmax_t>(length) > maxBytes
        || static_cast<std::uintmax_t>(length) > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        error = {ResourceErrorCode::LimitExceeded, "Resource exceeds the file size limit: " + name};
        return false;
    }
    try {
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
        input.seekg(0);
        if (!bytes.empty()) {
            input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        if (!input || input.peek() != std::char_traits<char>::eof()) {
            error = {ResourceErrorCode::Io, "Cannot read resource completely: " + name};
            return false;
        }
        result = std::move(bytes);
    } catch (const std::bad_alloc&) {
        error = {ResourceErrorCode::LimitExceeded, "Not enough memory to read resource: " + name};
        return false;
    }
    error = {};
    return true;
}

bool ResourceFiles::readText(const std::string& name, std::string& result,
    ResourceError& error, std::size_t maxBytes) const
{
    std::vector<std::uint8_t> bytes;
    if (!readBinary(name, bytes, error, maxBytes)) {
        return false;
    }
    try {
        std::string text(bytes.begin(), bytes.end());
        result = std::move(text);
    } catch (const std::bad_alloc&) {
        error = {ResourceErrorCode::LimitExceeded, "Not enough memory for resource text: " + name};
        return false;
    }
    return true;
}

bool ResourceFiles::loadPng(const std::string& name, Image& result,
    ResourceError& error, const ResourceLimits& limits) const
{
    std::vector<std::uint8_t> bytes;
    if (!readBinary(name, bytes, error, limits.fileBytes)) {
        return false;
    }
    if (!decodePng(bytes, result, error, limits)) {
        error.message = name + ": " + error.message;
        return false;
    }
    return true;
}
}
