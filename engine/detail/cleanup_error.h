#pragma once

#include "app/platform.h"

#include <utility>

namespace Platform::Detail
{
// Основная ошибка возвращается вызывающему коду, последующие доступны в stderr.
inline void keepFirstError(Error& first, Error next)
{
    if (first.operation.empty()) {
        first = std::move(next);
    } else {
        reportError(next, false);
    }
}

// Откатывает частично созданную платформу при отказе или исключении.
struct StartupRollback
{
    ~StartupRollback()
    {
        if (!committed) {
            Error error;
            if (!shutdown(error)) {
                reportError(error, false);
            }
        }
    }

    bool committed = false;
};
}
