#include "platform.h"

#include <iostream>
#include <vector>

int main()
{
    std::vector<Platform::Error> errors = {
        { "previous operation", "Previous error", std::nullopt }
    };
    Platform::shutdown(errors);
    if (!errors.empty()) {
        std::cerr << "Shutdown without initialization must leave no errors.\n";
        return 1;
    }

    Platform::shutdown(errors);
    if (!errors.empty()) {
        std::cerr << "Repeated shutdown must leave no errors.\n";
        return 1;
    }
    return 0;
}
