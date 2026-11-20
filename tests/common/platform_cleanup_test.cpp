#include "app/platform.h"

#include <iostream>
#include <vector>

int main()
{
    Platform::Error failure = {"previous operation", "Previous error", std::nullopt};
    Platform::shutdown(failure);
    if (!failure.operation.empty()) {
        std::cerr << "Shutdown without initialization must leave no errors.\n";
        return 1;
    }

    Platform::shutdown(failure);
    if (!failure.operation.empty()) {
        std::cerr << "Repeated shutdown must leave no errors.\n";
        return 1;
    }
    return 0;
}
