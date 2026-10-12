#include "platform.h"

// windows.h предоставляет окна, сообщения, устройства ввода и WGL;
// mmsystem.h объявляет функции точности системного таймера;
// gl.h содержит используемые приложением команды OpenGL.
#include <windows.h>
#include <mmsystem.h>
#include <GL/gl.h>

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

// LRESULT — результат обработки сообщения; CALLBACK задаёт соглашение вызова Win32.
// HWND — окно-получатель; UINT — код сообщения; WPARAM и LPARAM — его дополнительные данные.
LRESULT CALLBACK windowProcedure(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{
    // WM_CLOSE — запрос закрытия окна от пользователя; DestroyWindow начинает его уничтожение.
    // После этого Windows отправит WM_DESTROY.
    if (message == WM_CLOSE) {
        DestroyWindow(handle);
        return 0;
    }

    // WM_DESTROY означает, что окно уничтожено; PostQuitMessage кладёт WM_QUIT в очередь потока.
    // processEvents извлечёт WM_QUIT и завершит игровой цикл.
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    // DefWindowProc выполняет стандартное поведение Windows для необработанных сообщений.
    return DefWindowProc(handle, message, wParam, lParam);
}

bool initializeWindows(int width, int height)
{
    // HINSTANCE — дескриптор текущего приложения; GetModuleHandle(nullptr) возвращает
    // экземпляр исполняемого файла, уже загруженный Windows.
    HINSTANCE instance = GetModuleHandle(nullptr);

    // WNDCLASS описывает класс будущих окон: обработчик, курсор и имя класса.
    WNDCLASS windowClass = {};
    // CS_OWNDC просит Windows выделить окну постоянный собственный HDC для OpenGL.
    windowClass.style = CS_OWNDC;
    windowClass.lpfnWndProc = windowProcedure; // Функция обратного вызова для сообщений окна.
    windowClass.hInstance = instance;          // экземпляр программы-владельца класса.
    // LoadCursor загружает системный курсор; IDC_ARROW выбирает стандартную стрелку.
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW); // nullptr выбирает системный ресурс.
    windowClass.lpszClassName = "OpenGLDemoWindow";       // имя для CreateWindow.
    // RegisterClass публикует описание WNDCLASS в системе; без регистрации CreateWindow не найдёт класс.
    if (!RegisterClass(&windowClass)) {
        return false;
    }

    // CreateWindow создаёт окно по зарегистрированному имени.
    // WS_OVERLAPPEDWINDOW включает рамку, заголовок, системное меню и кнопки управления окном.
    // Координаты 100, 100 задают начальное положение; width и height — размер окна.
    // nullptr для родителя и меню создаёт самостоятельное окно без меню приложения.
    window = CreateWindow(windowClass.lpszClassName, "ORBITAL // SECTOR 09",
        WS_OVERLAPPEDWINDOW, 100, 100, width, height, nullptr, nullptr, instance, nullptr);
    if (!window) {
        return false;
    }

    // GetDC получает HDC клиентской области — поверхности, куда OpenGL будет выводить кадры.
    deviceContext = GetDC(window);

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
    if (pixelFormat == 0 || !SetPixelFormat(deviceContext, pixelFormat, &format)) {
        return false;
    }

    // wglCreateContext создаёт контекст WGL для HDC; wglMakeCurrent назначает его текущим потоку.
    // Все последующие вызовы OpenGL из этого потока будут рисовать в эту поверхность.
    openGLContext = wglCreateContext(deviceContext);
    if (!openGLContext || !wglMakeCurrent(deviceContext, openGLContext)) {
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

// Общая функция интерфейса передаёт работу закрытой реализации Win32.
bool initialize(int width, int height)
{
    return initializeWindows(width, height);
}

void setWindowTitle(const char* title)
{
    // SetWindowTextA задаёт заголовок в кодировке ANSI; HWND проверяется на случай
    // вызова до создания окна или после его закрытия.
    if (window) {
        SetWindowTextA(window, title); // Суффикс A выбирает строку ANSI; API также имеет Unicode-вариант W.
    }
}

bool processEvents()
{
    // MSG — структура одного сообщения Win32: код, окно и параметры события.
    MSG message;
    // Забираем все накопившиеся сообщения, чтобы окно отвечало на ввод и перерисовку.
    // PeekMessage не блокирует поток; PM_REMOVE извлекает найденное сообщение из очереди.
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
        // WM_QUIT — специальное сообщение завершения потока сообщений.
        if (message.message == WM_QUIT) {
            return false;
        }

        // TranslateMessage создаёт символьные сообщения из клавиатурных кодов,
        // а DispatchMessage передаёт сообщение оконной процедуре.
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    // GetAsyncKeyState читает состояние клавиши асинхронно; VK_ESCAPE — виртуальный код Escape.
    // Маска 0x8000 проверяет старший бит «клавиша удерживается».
    return (GetAsyncKeyState(VK_ESCAPE) & 0x8000) == 0;
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

void present()
{
    // SwapBuffers показывает задний буфер HDC и делает прежний передний буфер новым задним.
    SwapBuffers(deviceContext);
}

void shutdown()
{
    // timeEndPeriod(1) снимает запрос точности, сделанный timeBeginPeriod(1).
    if (timerResolutionEnabled) {
        timeEndPeriod(1);
        timerResolutionEnabled = false;
    }

    // wglMakeCurrent(nullptr, nullptr) отвязывает контекст от потока;
    // wglDeleteContext освобождает созданный WGL-контекст.
    if (openGLContext) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(openGLContext);
        openGLContext = nullptr;
    }

    // ReleaseDC возвращает HDC, выданный GetDC; DestroyWindow освобождает HWND и связанные ресурсы.
    if (deviceContext && window) {
        ReleaseDC(window, deviceContext);
        deviceContext = nullptr;
    }
    if (window) {
        DestroyWindow(window);
        window = nullptr;
    }
}
}
