#include "platform.h"

#include <iostream>
#include <vector>

int main()
{
    // Платформа не инициализирована: освобождать ресурсы не требуется.
    // Старое содержимое выходного аргумента не должно считаться ошибкой нового вызова.
    std::vector<Platform::Error> errors = {
        { "previous operation", "Previous error", std::nullopt }
    };
    Platform::shutdown(errors);
    if (!errors.empty()) {
        std::cerr << "Shutdown without initialization must leave no errors.\n";
        return 1;
    }

    // Повторная очистка должна быть безопасной и не создавать ложных ошибок.
    Platform::shutdown(errors);
    if (!errors.empty()) {
        std::cerr << "Repeated shutdown must leave no errors.\n";
        return 1;
    }
    return 0;
}
