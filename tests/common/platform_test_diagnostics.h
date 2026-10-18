#pragma once

#include "platform.h"

#include <vector>

namespace PlatformTest
{
// Выводить полученную ошибку только при неожиданном отказе операции.
inline bool checkOperation(bool success, const Platform::Error& error)
{
    if (!success) {
        Platform::reportError(error, false);
    }
    return success;
}

inline void reportErrors(const std::vector<Platform::Error>& errors)
{
    for (const auto& error : errors) {
        Platform::reportError(error, false);
    }
}

// Исходные ошибки выводятся до очистки; ошибки очистки сохраняются в отдельном списке.
inline void shutdownAfterFailure(const std::vector<Platform::Error>& errors)
{
    reportErrors(errors);
    std::vector<Platform::Error> cleanupErrors;
    Platform::shutdown(cleanupErrors);
    reportErrors(cleanupErrors);
}
}
