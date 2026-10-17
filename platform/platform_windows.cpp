#include "platform.h"

// windows.h предоставляет окна, сообщения, устройства ввода и WGL;
// mmsystem.h объявляет функции точности системного таймера;
// gl.h содержит используемые приложением команды OpenGL.
#include <windows.h>
#include <mmsystem.h>
#include <GL/gl.h>
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
constexpr const char* WindowClassName = "OpenGLDemoWindow";
bool windowClassRegistered = false;
bool closeRequested = false;

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

// LRESULT — результат обработки сообщения; CALLBACK задаёт соглашение вызова Win32.
// HWND — окно-получатель; UINT — код сообщения; WPARAM и LPARAM — его дополнительные данные.
LRESULT CALLBACK windowProcedure(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{
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
        }
        PostQuitMessage(0);
        return 0;
    }

    // DefWindowProc выполняет стандартное поведение Windows для необработанных сообщений.
    return DefWindowProc(handle, message, wParam, lParam);
}

bool initializeWindows(int width, int height, Error& error)
{
    // HINSTANCE — дескриптор текущего приложения; GetModuleHandle(nullptr) возвращает
    // экземпляр исполняемого файла, уже загруженный Windows.
    closeRequested = false;
    applicationInstance = GetModuleHandle(nullptr);
    if (!applicationInstance) {
        error = windowsError("GetModuleHandle", "Could not get the application module.", GetLastError());
        return false;
    }

    // WNDCLASS описывает класс будущих окон: обработчик, курсор и имя класса.
    WNDCLASS windowClass = {};
    // CS_OWNDC просит Windows выделить окну постоянный собственный HDC для OpenGL.
    windowClass.style = CS_OWNDC;
    windowClass.lpfnWndProc = windowProcedure; // Функция обратного вызова для сообщений окна.
    windowClass.hInstance = applicationInstance;          // экземпляр программы-владельца класса.
    // LoadCursor загружает системный курсор; IDC_ARROW выбирает стандартную стрелку.
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW); // nullptr выбирает системный ресурс.
    windowClass.lpszClassName = WindowClassName;       // имя для CreateWindow.
    // RegisterClass публикует описание WNDCLASS в системе; без регистрации CreateWindow не найдёт класс.
    if (!RegisterClass(&windowClass)) {
        error = windowsError("RegisterClass", "Could not register the window class.", GetLastError());
        return false;
    }

    windowClassRegistered = true;

    // CreateWindow создаёт окно по зарегистрированному имени.
    // WS_OVERLAPPEDWINDOW включает рамку, заголовок, системное меню и кнопки управления окном.
    // Координаты 100, 100 задают начальное положение; width и height — размер окна.
    // nullptr для родителя и меню создаёт самостоятельное окно без меню приложения.
    window = CreateWindow(windowClass.lpszClassName, "ORBITAL // SECTOR 09",
        WS_OVERLAPPEDWINDOW, 100, 100, width, height, nullptr, nullptr, applicationInstance, nullptr);
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
    return true;
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

bool initialize(int width, int height, Error& error)
{
    return initializeWindows(width, height, error);
}

bool setWindowTitle(const char* title, Error& error)
{
    if (!SetWindowTextA(window, title)) {
        error = windowsError("SetWindowTextA", "Could not update the window title.", GetLastError());
        return false;
    }
    return true;
}

EventResult processEvents(Error&)
{
    // MSG — структура одного сообщения Win32: код, окно и параметры события.
    MSG message;
    // Забираем все накопившиеся сообщения, чтобы окно отвечало на ввод и перерисовку.
    // PeekMessage не блокирует поток; PM_REMOVE извлекает найденное сообщение из очереди.
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
        // WM_QUIT — специальное сообщение завершения потока сообщений.
        if (message.message == WM_QUIT) {
            return EventResult::Quit;
        }

        // TranslateMessage создаёт символьные сообщения из клавиатурных кодов,
        // а DispatchMessage передаёт сообщение оконной процедуре.
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    // GetAsyncKeyState читает состояние клавиши асинхронно; VK_ESCAPE — виртуальный код Escape.
    // Маска 0x8000 проверяет старший бит «клавиша удерживается».
    return closeRequested || (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0
        ? EventResult::Quit : EventResult::Continue;
}

bool isKeyDown(Key key)
{
    // VK_SPACE и VK_UP/VK_DOWN/VK_LEFT/VK_RIGHT — виртуальные коды Windows.
    // Для WASD используются символьные коды 'W'/'A'/'S'/'D'; ноль означает отсутствие альтернативы.
    int primary = VK_SPACE;
    int alternate = 0;
    switch (key) {
        case Key::Up:
            primary = VK_UP;
            alternate = 'W';
            break;
        case Key::Down:
            primary = VK_DOWN;
            alternate = 'S';
            break;
        case Key::Left:
            primary = VK_LEFT;
            alternate = 'A';
            break;
        case Key::Right:
            primary = VK_RIGHT;
            alternate = 'D';
            break;
        case Key::Fire:
            break;
    }

    // GetAsyncKeyState возвращает битовую маску; 0x8000 проверяет удержание.
    // При alternate == 0 второй вызов не выполняется, поэтому Fire назначен только на пробел.
    const bool primaryPressed = (GetAsyncKeyState(primary) & 0x8000) != 0;
    const bool alternatePressed = alternate != 0 && (GetAsyncKeyState(alternate) & 0x8000) != 0;
    return primaryPressed || alternatePressed;
}

bool present(Error& error)
{
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
        if (!UnregisterClass(WindowClassName, applicationInstance)) {
            errors.push_back(windowsError("UnregisterClass", "Could not unregister the window class.", GetLastError()));
        }
        windowClassRegistered = false;
    }
    applicationInstance = nullptr;
    closeRequested = false;
}
}
