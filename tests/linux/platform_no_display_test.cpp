#include "platform.h"

#include <iostream>
#include <vector>

int main()
{
    // CTest запускает этот процесс с пустым DISPLAY. Ошибка должна возникнуть
    // в XOpenDisplay, до создания окна, без зависимости от приложения или игровой сцены.
    Platform::WindowOptions options;
    std::vector<Platform::Error> errors;
    if (Platform::initialize(options, errors)) {
        std::cerr << "Initialization without an X11 display must fail.\n";
        Platform::shutdown(errors);
        return 1;
    }
    if (errors.size() != 1 || errors.front().operation != "XOpenDisplay"
        || errors.front().message.empty()) {
        std::cerr << "Expected a single XOpenDisplay error.\n";
        for (const auto& error : errors) {
            Platform::reportError(error, false);
        }
        return 1;
    }
    Platform::WindowState state;
    Platform::Error error;
    if (Platform::getWindowState(state, error) || Platform::isCloseRequested()) {
        std::cerr << "Failed initialization must leave no usable platform state.\n";
        return 1;
    }
    return 0;
}
