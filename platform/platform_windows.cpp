#include "platform.h"
#include "window_validation.h"

// windows.h предоставляет окна, сообщения, устройства ввода и WGL;
// mmsystem.h объявляет функции точности системного таймера;
// gl.h содержит используемые приложением команды OpenGL.
#include <windows.h>
#include <mmsystem.h>
#include <GL/gl.h>
#include <array>
#include <iostream>

namespace Platform
{
namespace
{
// HWND — дескриптор окна Win32; HDC — дескриптор его графической поверхности;
// HGLRC — дескриптор контекста OpenGL, привязанного к этой поверхности.
HWND window = nullptr;
HDC deviceContext = nullptr;
HGLRC openGLContext = nullptr;

// Флаг нужен, чтобы завершить только успешно начатый запрос точности таймера.
bool timerResolutionEnabled = false;

HINSTANCE applicationInstance = nullptr;
constexpr const wchar_t* WindowClassName = L"EngineWindow";
bool windowClassRegistered = false;
bool closeRequested = false;
bool initialized = false;
WindowState windowState;
bool viewportNeedsUpdate = false;
// Win32 использует таблицу из 256 виртуальных кодов клавиш; старший бит означает удержание.
std::array<BYTE, 256> keyState = {};
WindowOptions windowOptions;
LONG minimumTrackWidth = 0;
LONG minimumTrackHeight = 0;

Error windowsError(const char* operation, const char* message, DWORD code = ERROR_SUCCESS)
{
    Error error = { operation, message, std::nullopt };
    if (code != ERROR_SUCCESS) {
        error.nativeCode = static_cast<int>(code);
        // ALLOCATE_BUFFER просит Windows выделить память нужного размера. API требует
        // передать адрес указателя через LPWSTR; освобождать память нужно только LocalFree.
        wchar_t* description = nullptr;
        const DWORD descriptionLength = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, code, 0, reinterpret_cast<LPWSTR>(&description), 0, nullptr);
        if (descriptionLength != 0) {
            // Первый вызов определяет число UTF-8 байт, второй заполняет строку точно этого размера.
            const int length = WideCharToMultiByte(CP_UTF8, 0, description,
                static_cast<int>(descriptionLength), nullptr, 0, nullptr, nullptr);
            if (length > 0) {
                std::string utf8Description(static_cast<std::size_t>(length), '\0');
                if (WideCharToMultiByte(CP_UTF8, 0, description,
                    static_cast<int>(descriptionLength), utf8Description.data(), length, nullptr, nullptr) != 0) {
                    error.message += " ";
                    error.message += utf8Description;
                }
            }
        }
        if (description) {
            LocalFree(description);
        }
    }
    return error;
}

bool convertTitle(const std::string& title, std::wstring& wideTitle, Error& error)
{
    if (title.empty()) {
        wideTitle.clear();
        return true;
    }
    // Сначала определяем размер UTF-16, затем выполняем преобразование в выделенную строку.
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title.data(),
        static_cast<int>(title.size()), nullptr, 0);
    if (length == 0) {
        error = windowsError("MultiByteToWideChar", "Could not convert the window title.", GetLastError());
        return false;
    }
    wideTitle.resize(static_cast<std::size_t>(length));
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title.data(),
        static_cast<int>(title.size()), wideTitle.data(), length) == 0) {
        error = windowsError("MultiByteToWideChar", "Could not convert the window title.", GetLastError());
        return false;
    }
    return true;
}

// LRESULT — результат обработки сообщения; CALLBACK задаёт соглашение вызова Win32.
// HWND — окно-получатель; UINT — код сообщения; WPARAM и LPARAM — его дополнительные данные.
LRESULT CALLBACK windowProcedure(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{
    // WM_GETMINMAXINFO задаёт ограничения внешней рамки, хотя API движка принимает клиентские размеры.
    if (message == WM_GETMINMAXINFO) {
        const LRESULT result = DefWindowProcW(handle, message, wParam, lParam);
        auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
        if (!windowOptions.resizable || windowOptions.minimumWidth > 0) {
            limits->ptMinTrackSize.x = minimumTrackWidth;
        }
        if (!windowOptions.resizable || windowOptions.minimumHeight > 0) {
            limits->ptMinTrackSize.y = minimumTrackHeight;
        }
        if (!windowOptions.resizable) {
            limits->ptMaxTrackSize.x = minimumTrackWidth;
            limits->ptMaxTrackSize.y = minimumTrackHeight;
        }
        return result;
    }

    // Эти сообщения уже содержат новое состояние: опрос ОС на каждом кадре не требуется.
    if (message == WM_SIZE) {
        const int width = static_cast<int>(LOWORD(lParam));
        const int height = static_cast<int>(HIWORD(lParam));
        viewportNeedsUpdate = viewportNeedsUpdate || width != windowState.width || height != windowState.height;
        windowState.width = width;
        windowState.height = height;
        windowState.minimized = wParam == SIZE_MINIMIZED;
        if (windowState.minimized) {
            windowState.focused = false;
        }
        return 0;
    }
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS) {
        windowState.focused = message == WM_SETFOCUS && !windowState.minimized;
        return 0;
    }

    // WM_CLOSE только запрашивает выход. Контекст и HDC освобождаются до уничтожения окна в shutdown().
    if (message == WM_CLOSE) {
        closeRequested = true;
        return 0;
    }

    // WM_DESTROY означает, что окно уничтожено; PostQuitMessage кладёт WM_QUIT в очередь потока.
    // processEvents извлечёт WM_QUIT и завершит игровой цикл.
    if (message == WM_DESTROY) {
        if (handle == window) {
            window = nullptr;
            deviceContext = nullptr; // При внешнем уничтожении окна его частный DC уже недоступен.
            closeRequested = true;
            windowState.width = 0;
            windowState.height = 0;
            windowState.focused = false;
            windowState.minimized = false;
        }
        PostQuitMessage(0);
        return 0;
    }

    // DefWindowProc выполняет стандартное поведение Windows для необработанных сообщений.
    return DefWindowProcW(handle, message, wParam, lParam);
}

bool requireWindow(const char* operation, Error& error)
{
    if (!initialized || !window) {
        error = { operation, "The window is not initialized or has been destroyed.", std::nullopt };
        return false;
    }
    return true;
}

bool initializeWindows(const WindowOptions& options, Error& error)
{
    std::wstring title;
    if (!convertTitle(options.title, title, error)) {
        return false;
    }
    windowOptions = options;
    windowState = {};
    // AdjustWindowRectEx добавляет рамку и заголовок к запрошенной клиентской области.
    const DWORD style = options.resizable ? WS_OVERLAPPEDWINDOW
        : WS_OVERLAPPEDWINDOW & ~static_cast<DWORD>(WS_THICKFRAME | WS_MAXIMIZEBOX);
    RECT bounds = { 0, 0, options.width, options.height };
    RECT minimumBounds = { 0, 0, options.resizable ? options.minimumWidth : options.width,
        options.resizable ? options.minimumHeight : options.height };
    if (!AdjustWindowRectEx(&bounds, style, FALSE, 0)
        || !AdjustWindowRectEx(&minimumBounds, style, FALSE, 0)) {
        error = windowsError("AdjustWindowRectEx", "Could not calculate the window frame size.", GetLastError());
        return false;
    }
    minimumTrackWidth = minimumBounds.right - minimumBounds.left;
    minimumTrackHeight = minimumBounds.bottom - minimumBounds.top;
    // shutdown может оставить WM_QUIT от уничтоженного окна в очереди текущего потока.
    MSG oldQuit;
    while (PeekMessage(&oldQuit, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) {
    }
    // HINSTANCE — дескриптор текущего приложения; GetModuleHandle(nullptr) возвращает
    // экземпляр исполняемого файла, уже загруженный Windows.
    closeRequested = false;
    applicationInstance = GetModuleHandle(nullptr);
    if (!applicationInstance) {
        error = windowsError("GetModuleHandle", "Could not get the application module.", GetLastError());
        return false;
    }

    // WNDCLASS описывает класс будущих окон: обработчик, курсор и имя класса.
    WNDCLASSW windowClass = {};
    // CS_OWNDC просит Windows выделить окну постоянный собственный HDC для OpenGL.
    windowClass.style = CS_OWNDC;
    windowClass.lpfnWndProc = windowProcedure; // Функция обратного вызова для сообщений окна.
    windowClass.hInstance = applicationInstance;          // экземпляр программы-владельца класса.
    // LoadCursor загружает системный курсор; IDC_ARROW выбирает стандартную стрелку.
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW); // nullptr выбирает системный ресурс.
    windowClass.lpszClassName = WindowClassName;       // имя для CreateWindow.
    // RegisterClass публикует описание WNDCLASS в системе; без регистрации CreateWindow не найдёт класс.
    if (!RegisterClassW(&windowClass)) {
        error = windowsError("RegisterClass", "Could not register the window class.", GetLastError());
        return false;
    }

    windowClassRegistered = true;

    // CreateWindow создаёт окно по зарегистрированному имени.
    // Для фиксированного размера исключены растягиваемая рамка и кнопка разворачивания.
    // Координаты 100, 100 задают начальное положение; bounds включает рамку и заголовок.
    // nullptr для родителя и меню создаёт самостоятельное окно без меню приложения.
    window = CreateWindowW(windowClass.lpszClassName, title.c_str(),
        style, 100, 100, bounds.right - bounds.left, bounds.bottom - bounds.top,
        nullptr, nullptr, applicationInstance, nullptr);
    if (!window) {
        error = windowsError("CreateWindow", "Could not create the application window.", GetLastError());
        return false;
    }

    // GetDC получает HDC клиентской области — поверхности, куда OpenGL будет выводить кадры.
    deviceContext = GetDC(window);
    if (!deviceContext) {
        // GetDC не документирует расширенную ошибку через GetLastError.
        error = windowsError("GetDC", "Could not get the window drawing surface.");
        return false;
    }

    // PIXELFORMATDESCRIPTOR описывает требования к пикселям поверхности.
    // PFD_DRAW_TO_WINDOW разрешает вывод в окно; PFD_SUPPORT_OPENGL — использование OpenGL;
    // PFD_DOUBLEBUFFER включает задний буфер; PFD_TYPE_RGBA задаёт компоненты RGBA.
    PIXELFORMATDESCRIPTOR format = {};
    format.nSize = sizeof(format); // Размер структуры нужен Win32 для проверки версии описания.
    format.nVersion = 1;           // Версия PIXELFORMATDESCRIPTOR.
    format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    format.iPixelType = PFD_TYPE_RGBA; // Тип пикселей RGBA; точность альфа-канала отдельно не запрашивается.
    format.cColorBits = 32; // cColorBits задаёт число бит цветового буфера на пиксель.
    format.cDepthBits = 24; // cDepthBits задаёт точность буфера глубины.
    // ChoosePixelFormat выбирает поддерживаемый драйвером формат, ближайший к запросу.
    const int pixelFormat = ChoosePixelFormat(deviceContext, &format);

    // SetPixelFormat закрепляет выбранный формат за HDC; формат поверхности после этого неизменяем.
    if (pixelFormat == 0) {
        error = windowsError("ChoosePixelFormat", "No compatible OpenGL pixel format was found.", GetLastError());
        return false;
    }
    if (!SetPixelFormat(deviceContext, pixelFormat, &format)) {
        error = windowsError("SetPixelFormat", "Could not set the OpenGL pixel format.", GetLastError());
        return false;
    }

    // wglCreateContext создаёт контекст WGL для HDC; wglMakeCurrent назначает его текущим потоку.
    // Все последующие вызовы OpenGL из этого потока будут рисовать в эту поверхность.
    openGLContext = wglCreateContext(deviceContext);
    if (!openGLContext) {
        error = windowsError("wglCreateContext", "Could not create the OpenGL context.", GetLastError());
        return false;
    }
    if (!wglMakeCurrent(deviceContext, openGLContext)) {
        error = windowsError("wglMakeCurrent", "Could not activate the OpenGL context.", GetLastError());
        return false;
    }

    // timeBeginPeriod(1) просит Multimedia Timer использовать точность 1 мс.
    // TIMERR_NOERROR означает успех; timeEndPeriod(1) парно отменит запрос в shutdown.
    timerResolutionEnabled = timeBeginPeriod(1) == TIMERR_NOERROR;

    // ShowWindow показывает готовое окно; SW_SHOW означает обычный показ.
    ShowWindow(window, SW_SHOW);
    // Начальный снимок запрашивается один раз; дальнейшие изменения приходят в оконную процедуру.
    RECT clientBounds = {};
    if (!GetClientRect(window, &clientBounds)) {
        error = windowsError("GetClientRect", "Could not read the initial client dimensions.", GetLastError());
        return false;
    }
    windowState = { static_cast<int>(clientBounds.right - clientBounds.left),
        static_cast<int>(clientBounds.bottom - clientBounds.top), GetFocus() == window, IsIconic(window) != FALSE };
    viewportNeedsUpdate = true;
    initialized = true;
    return processEvents(error);
}
}

// Диагностика отделена от обнаружения ошибок системных операций.
void reportError(const Error& error, bool showDialog)
{
    std::string text = "OpenGLDemo: " + error.operation + ": " + error.message;
    if (error.nativeCode) {
        text += " (code " + std::to_string(*error.nativeCode) + ")";
    }
    std::cerr << text << '\n';
    if (showDialog) {
        // Строки Error хранятся в UTF-8, а MessageBoxW принимает UTF-16.
        const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (length > 0) {
            std::wstring wideText(static_cast<std::size_t>(length), L'\0');
            if (MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wideText.data(), length) != 0) {
                // nullptr не требует существующего окна демо или OpenGL-контекста.
                if (MessageBoxW(nullptr, wideText.c_str(), L"OpenGLDemo - Error",
                    MB_OK | MB_ICONERROR | MB_TASKMODAL) == 0) {
                    const DWORD code = GetLastError();
                    std::cerr << "OpenGLDemo: MessageBoxW failed (code " << code << ").\n";
                }
            }
        }
    }
}

bool initialize(const WindowOptions& options, std::vector<Error>& errors)
{
    errors.clear();
    if (initialized || windowClassRegistered || window || openGLContext) {
        errors.push_back({ "initialize", "Call shutdown before initializing again.", std::nullopt });
        return false;
    }
    Error error;
    if (!validateWindowOptions(options, error)) {
        errors.push_back(error);
        return false;
    }
    if (initializeWindows(options, error)) {
        return true;
    }
    // Сохраняем исходную причину перед очисткой: shutdown заменяет переданный список.
    shutdown(errors);
    errors.insert(errors.begin(), error);
    return false;
}

bool getWindowState(WindowState& state, Error& error)
{
    if (!initialized) {
        error = { "getWindowState", "The platform is not initialized.", std::nullopt };
        return false;
    }
    state = windowState;
    return true;
}

bool setWindowTitle(const std::string& title, Error& error)
{
    if (!requireWindow("setWindowTitle", error) || !validateWindowTitle(title, "setWindowTitle", error)) {
        return false;
    }
    std::wstring wideTitle;
    if (!convertTitle(title, wideTitle, error)) {
        return false;
    }
    if (!SetWindowTextW(window, wideTitle.c_str())) {
        error = windowsError("SetWindowTextW", "Could not update the window title.", GetLastError());
        return false;
    }
    return true;
}

bool processEvents(Error& error)
{
    if (initialized && !window && closeRequested) {
        return true;
    }
    if (!requireWindow("processEvents", error)) {
        return false;
    }
    // MSG — структура одного сообщения Win32: код, окно и параметры события.
    MSG message;
    // Забираем все накопившиеся сообщения, чтобы окно отвечало на ввод и перерисовку.
    // PeekMessage не блокирует поток; PM_REMOVE извлекает найденное сообщение из очереди.
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
        // WM_QUIT — специальное сообщение завершения потока сообщений.
        if (message.message == WM_QUIT) {
            closeRequested = true;
            return true;
        }

        // TranslateMessage создаёт символьные сообщения из клавиатурных кодов,
        // а DispatchMessage передаёт сообщение оконной процедуре.
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    // Область вывода занимает клиентскую область; матрицы игрового мира не меняются.
    if (window && viewportNeedsUpdate) {
        glViewport(0, 0, windowState.width, windowState.height);
        viewportNeedsUpdate = false;
    }
    // Обновляем снимок клавиатуры после обработки накопленных событий.
    // GetKeyboardState читает состояние очереди текущего потока; PeekMessage выше обновляет его.
    if (window && windowState.focused && !windowState.minimized) {
        if (!GetKeyboardState(keyState.data())) {
            error = windowsError("GetKeyboardState", "Could not read the keyboard state.", GetLastError());
            return false;
        }
    }
    return true;
}

bool isCloseRequested()
{
    return initialized && closeRequested;
}

bool isKeyDown(Key key)
{
    if (!initialized || !window || !windowState.focused || windowState.minimized) {
        return false;
    }
    int nativeKey = 0;
    switch (key) {
        case Key::ArrowUp:
            nativeKey = VK_UP;
            break;
        case Key::ArrowDown:
            nativeKey = VK_DOWN;
            break;
        case Key::ArrowLeft:
            nativeKey = VK_LEFT;
            break;
        case Key::ArrowRight:
            nativeKey = VK_RIGHT;
            break;
        case Key::W:
            nativeKey = 'W';
            break;
        case Key::A:
            nativeKey = 'A';
            break;
        case Key::S:
            nativeKey = 'S';
            break;
        case Key::D:
            nativeKey = 'D';
            break;
        case Key::Space:
            nativeKey = VK_SPACE;
            break;
        case Key::Escape:
            nativeKey = VK_ESCAPE;
            break;
        default:
            return false;
    }
    return (keyState[static_cast<std::size_t>(nativeKey)] & 0x80U) != 0;
}


bool present(Error& error)
{
    if (!requireWindow("present", error)) {
        return false;
    }
    if (windowState.minimized) {
        return true;
    }
    // SwapBuffers сообщает об отказе синхронно; уведомление выполняет вызывающий код.
    if (!SwapBuffers(deviceContext)) {
        error = windowsError("SwapBuffers", "Could not present the frame.", GetLastError());
        return false;
    }
    return true;
}

void shutdown(std::vector<Error>& errors)
{
    errors.clear();
    if (timerResolutionEnabled) {
        // Multimedia API возвращает собственный код, а не GetLastError.
        const MMRESULT result = timeEndPeriod(1);
        if (result != TIMERR_NOERROR) {
            errors.push_back({ "timeEndPeriod", "Could not restore the timer resolution.", static_cast<int>(result) });
        }
        timerResolutionEnabled = false;
    }
    if (openGLContext) {
        if (!wglMakeCurrent(nullptr, nullptr)) {
            errors.push_back(windowsError("wglMakeCurrent (detach)", "Could not detach the OpenGL context.", GetLastError()));
        }
        if (!wglDeleteContext(openGLContext)) {
            errors.push_back(windowsError("wglDeleteContext", "Could not delete the OpenGL context.", GetLastError()));
        }
        openGLContext = nullptr;
    }
    if (deviceContext && window) {
        // ReleaseDC не предоставляет расширенную ошибку через GetLastError.
        if (!ReleaseDC(window, deviceContext)) {
            errors.push_back(windowsError("ReleaseDC", "Could not release the window drawing surface."));
        }
        deviceContext = nullptr;
    }
    if (window) {
        if (!DestroyWindow(window)) {
            errors.push_back(windowsError("DestroyWindow", "Could not destroy the application window.", GetLastError()));
        }
        window = nullptr;
    }
    if (windowClassRegistered) {
        if (!UnregisterClassW(WindowClassName, applicationInstance)) {
            errors.push_back(windowsError("UnregisterClass", "Could not unregister the window class.", GetLastError()));
        }
        windowClassRegistered = false;
    }
    applicationInstance = nullptr;
    closeRequested = false;
    initialized = false;
    windowState = {};
    viewportNeedsUpdate = false;
    keyState = {};
    windowOptions = {};
    minimumTrackWidth = 0;
    minimumTrackHeight = 0;
}
}
