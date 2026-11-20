#include "app/platform.h"

#include <iostream>

int main()
{
    // CTest запускает этот процесс с пустым DISPLAY. Ошибка должна возникнуть
    // в XOpenDisplay, до создания окна, без зависимости от приложения или игровой сцены.
    Platform::WindowOptions options;
    Platform::Error failure;
    if (Platform::initialize(options, failure)) {
        std::cerr << "Initialization without an X11 display must fail.\n";
        Platform::shutdown(failure);
        return 1;
    }
    if (failure.operation.empty() || failure.operation != "XOpenDisplay"
        || failure.message.empty()) {
        std::cerr << "Expected a single XOpenDisplay error.\n";
        Platform::reportError(failure, false);
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
