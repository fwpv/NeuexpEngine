#include "platform.h"

#include <iostream>
#include <limits>
#include <vector>

int main()
{
    Platform::Error error;
    const auto checkUnavailable = [&]() {
        Platform::WindowState state = { 123, 456, true, true };
        if (Platform::getWindowState(state, error) || error.operation != "getWindowState"
            || error.message.empty() || state.width != 123 || state.height != 456
            || !state.focused || !state.minimized || Platform::isCloseRequested()) {
            return false;
        }
        return !Platform::setWindowTitle("test", error)
            && !Platform::present(error)
            && Platform::processEvents(error) == Platform::EventResult::Failed
            && !Platform::isKeyDown(Platform::Key::Fire);
    };
    if (!checkUnavailable()) {
        std::cerr << "Calls before initialization must fail safely without changing output.\n";
        return 1;
    }
    const Platform::WindowOptions defaults;
    if (defaults.minimumWidth != 0 || defaults.minimumHeight != 0) {
        std::cerr << "Minimum dimensions must default to zero.\n";
        return 1;
    }
    const int huge = std::numeric_limits<int>::max();
    const std::vector<Platform::WindowOptions> invalidOptions = {
        { 0, 240, 0, 0 }, { 320, 0, 0, 0 },
        { -1, 240, 0, 0 }, { 320, -1, 0, 0 },
        { 320, 240, -1, 0 }, { 320, 240, 0, -1 },
        { 320, 240, 321, 0 }, { 320, 240, 0, 241 },
        { 65536, 240, 0, 0 }, { 320, 65536, 0, 0 },
        { huge, huge, 0, 0 }, { 320, 240, huge, huge }
    };
    for (const auto& options : invalidOptions) {
        // Ошибочные размеры отклоняются до обращения к ОС, даже без графической сессии.
        if (Platform::initialize(options, error) || error.operation != "initialize" || error.message.empty()) {
            std::cerr << "Invalid dimensions must fail with an initialization error.\n";
            return 1;
        }
        std::vector<Platform::Error> cleanupErrors;
        Platform::shutdown(cleanupErrors);
        if (!cleanupErrors.empty() || !checkUnavailable()) {
            std::cerr << "Invalid initialization must not leave resources or a usable window.\n";
            return 1;
        }
    }
    return 0;
}
