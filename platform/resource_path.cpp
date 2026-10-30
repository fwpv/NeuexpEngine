#include "detail/executable_path.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <string>
#include <vector>

namespace Engine::Detail
{
bool executableDirectory(std::filesystem::path& result, ResourceError& error)
{
#ifdef _WIN32
    std::vector<wchar_t> buffer(256);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            error = {ResourceErrorCode::Io, "Cannot locate the executable (GetModuleFileNameW)."};
            return false;
        }
        if (length < buffer.size()) {
            result = std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
            return true;
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    std::vector<char> buffer(256);
    for (;;) {
        const auto length = readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (length < 0) {
            error = {ResourceErrorCode::Io, "Cannot locate the executable (/proc/self/exe)."};
            return false;
        }
        if (static_cast<std::size_t>(length) < buffer.size()) {
            result = std::filesystem::path(std::string(buffer.data(), static_cast<std::size_t>(length))).parent_path();
            return true;
        }
        buffer.resize(buffer.size() * 2);
    }
#endif
}
}
