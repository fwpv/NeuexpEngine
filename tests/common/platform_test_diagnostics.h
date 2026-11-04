#pragma once

#include "platform.h"

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

inline void reportError(const Platform::Error& failure)
{
    if (!failure.operation.empty()) {
        Platform::reportError(failure, false);
    }
}

// Исходная ошибка выводится до очистки, чтобы не потерять причину отказа.
inline void shutdownAfterFailure(const Platform::Error& failure)
{
    PlatformTest::reportError(failure);
    Platform::Error cleanupError;
    Platform::shutdown(cleanupError);
    PlatformTest::reportError(cleanupError);
}
}
