#include "platform.h"

// Xlib управляет окном и событиями, GLX связывает контекст OpenGL с X11.
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <cerrno>
#include <cstdlib>
#include <iostream>

namespace Platform
{
namespace
{
Display* display = nullptr;
Window window = 0;
Colormap colorMap = 0;
GLXContext openGLContext = nullptr;
Atom closeMessage = None;
Atom protocolsMessage = None;
XErrorHandler previousErrorHandler = nullptr;
XIOErrorHandler previousIOErrorHandler = nullptr;
bool handlersInstalled = false;

// Обработчик не выделяет память и не вызывает Xlib: сохраняет первую ещё не разобранную ошибку.
// Все вызовы платформы и разбор ошибок выполняются в одном, главном потоке.
XErrorEvent pendingError = {};

int recordError(Display*, XErrorEvent* event)
{
    if (pendingError.error_code == 0) {
        pendingError = *event;
    }
    return 0;
}

int connectionLost(Display*)
{
    // После ошибки ввода-вывода Xlib запрещает продолжать работу с соединением.
    // _Exit не запускает очистку или деструкторы, которые могли бы снова вызвать Xlib.
    const int systemError = errno;
    std::cerr << "OpenGLDemo: X11 connection lost (system error " << systemError << "). Cannot continue.\n";
    std::cerr.flush();
    std::_Exit(EXIT_FAILURE);
}

bool takeError(const char* operation, Error& error)
{
    if (pendingError.error_code == 0) {
        return true;
    }
    const XErrorEvent event = pendingError;
    pendingError = {};
    std::string description;
    if (display) {
        // XGetErrorText не умеет сообщать требуемый размер. Увеличиваем строку,
        // пока текст помещается с запасом для завершающего нуля.
        description.resize(128);
        while (true) {
            XGetErrorText(display, event.error_code, description.data(), static_cast<int>(description.size()));
            const std::size_t length = description.find('\0');
            if (length < description.size() - 1) {
                description.resize(length);
                break;
            }
            description.resize(description.size() * 2);
        }
    }
    // Название текущей операции — точка обнаружения. request/minor/serial указывают
    // исходный запрос, который мог быть отправлен раньше и обработан асинхронно.
    error = { operation, description + " (X11 request "
        + std::to_string(event.request_code) + "." + std::to_string(event.minor_code)
        + ", serial " + std::to_string(event.serial) + ")", event.error_code };
    return false;
}

bool synchronize(const char* operation, Error& error)
{
    // Синхронизация нужна при создании и удалении ресурсов, но не на каждом кадре.
    XSync(display, False);
    return takeError(operation, error);
}

bool initializeLinux(int width, int height, Error& error)
{
    // nullptr выбирает DISPLAY из окружения. Без соединения окно сообщения недоступно.
    display = XOpenDisplay(nullptr);
    if (!display) {
        error = { "XOpenDisplay", "Could not connect to the X server. Check DISPLAY and access to an X11 or XWayland session.", std::nullopt };
        return false;
    }
    // Обработчики глобальны для процесса. Сохраняем прежние и восстанавливаем после XCloseDisplay.
    previousErrorHandler = XSetErrorHandler(recordError);
    previousIOErrorHandler = XSetIOErrorHandler(connectionLost);
    handlersInstalled = true;
    pendingError = {};

    // None завершает список; запрашиваем RGBA, двойной буфер и глубину не меньше 24 бит.
    int attributes[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 24, None };
    XVisualInfo* visual = glXChooseVisual(display, DefaultScreen(display), attributes);
    const bool visualSucceeded = synchronize("glXChooseVisual", error);
    if (!visual || !visualSucceeded) {
        if (visual) {
            XFree(visual);
        }
        if (visualSucceeded) {
            error = { "glXChooseVisual", "No compatible OpenGL visual was found.", std::nullopt };
        }
        return false;
    }

    // Visual определяет формат окна; AllocNone не выделяет цветовые ячейки заранее.
    colorMap = XCreateColormap(display, RootWindow(display, visual->screen), visual->visual, AllocNone);
    const bool colorMapSucceeded = synchronize("XCreateColormap", error);
    if (!colorMap || !colorMapSucceeded) {
        colorMap = 0;
        XFree(visual);
        if (colorMapSucceeded) {
            error = { "XCreateColormap", "Could not create the window color map.", std::nullopt };
        }
        return false;
    }

    XSetWindowAttributes settings = {};
    settings.colormap = colorMap;
    settings.event_mask = KeyPressMask | StructureNotifyMask;
    // Маска указывает, какие поля settings используются при создании окна InputOutput.
    window = XCreateWindow(display, RootWindow(display, visual->screen), 100, 100, width, height,
        0, visual->depth, InputOutput, visual->visual, CWColormap | CWEventMask, &settings);
    const bool windowSucceeded = synchronize("XCreateWindow", error);
    if (!window || !windowSucceeded) {
        window = 0;
        XFree(visual);
        if (windowSucceeded) {
            error = { "XCreateWindow", "Could not create the application window.", std::nullopt };
        }
        return false;
    }

    // False разрешает создавать Atom, если такого имени ещё нет на сервере.
    XStoreName(display, window, "ORBITAL // SECTOR 09");
    protocolsMessage = XInternAtom(display, "WM_PROTOCOLS", False);
    closeMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
    const Status protocolsSet = XSetWMProtocols(display, window, &closeMessage, 1);
    const bool protocolsSucceeded = synchronize("XSetWMProtocols", error);
    if (!protocolsSet || closeMessage == None || protocolsMessage == None || !protocolsSucceeded) {
        XFree(visual);
        if (protocolsSucceeded) {
            error = { "XSetWMProtocols", "Could not configure the window close protocol.", std::nullopt };
        }
        return false;
    }

    XMapWindow(display, window);
    if (!synchronize("XMapWindow", error)) {
        XFree(visual);
        return false;
    }

    // True запрашивает прямой рендеринг; nullptr означает отсутствие разделяемого контекста.
    openGLContext = glXCreateContext(display, visual, nullptr, True);
    XFree(visual);
    const bool contextSucceeded = synchronize("glXCreateContext", error);
    if (!openGLContext || !contextSucceeded) {
        openGLContext = nullptr;
        if (contextSucceeded) {
            error = { "glXCreateContext", "Could not create the OpenGL context.", std::nullopt };
        }
        return false;
    }
    const Bool activated = glXMakeCurrent(display, window, openGLContext);
    const bool activationSucceeded = synchronize("glXMakeCurrent", error);
    if (!activated || !activationSucceeded) {
        if (activationSucceeded) {
            error = { "glXMakeCurrent", "Could not activate the OpenGL context.", std::nullopt };
        }
        return false;
    }
    return true;
}
}

void reportError(const Error& error, bool)
{
    std::cerr << "OpenGLDemo: " << error.operation << ": " << error.message;
    if (error.nativeCode) {
        std::cerr << " (code " << *error.nativeCode << ')';
    }
    std::cerr << '\n';
}

bool initialize(int width, int height, Error& error)
{
    return initializeLinux(width, height, error);
}

bool setWindowTitle(const char* title, Error&)
{
    XStoreName(display, window, title);
    XFlush(display);
    // X11 не подтверждает смену заголовка синхронно. Не забираем общую отложенную
    // ошибку здесь: она могла относиться к кадру и должна попасть в processEvents/present.
    return true;
}

EventResult processEvents(Error& error)
{
    if (!takeError("processEvents", error)) {
        return EventResult::Failed;
    }
    EventResult result = EventResult::Continue;
    // XPending читает доступные события и ошибки, не ожидая новых событий.
    while (XPending(display)) {
        XEvent event;
        XNextEvent(display, &event);
        if (event.type == DestroyNotify && event.xdestroywindow.window == window) {
            window = 0; // Не уничтожаем уже удалённое сервером окно второй раз.
            result = EventResult::Quit;
        }
        if (event.type == ClientMessage && event.xclient.window == window
            && event.xclient.message_type == protocolsMessage && event.xclient.format == 32
            && static_cast<Atom>(event.xclient.data.l[0]) == closeMessage) {
            result = EventResult::Quit;
        }
        if (event.type == KeyPress && XLookupKeysym(&event.xkey, 0) == XK_Escape) {
            result = EventResult::Quit;
        }
    }
    // Ошибка имеет приоритет над запросом закрытия, если они пришли в одной очереди.
    return takeError("processEvents", error) ? result : EventResult::Failed;
}

bool isKeyDown(Key key)
{
    // KeySym — символическое имя клавиши; XK_* константы задают стрелки, пробел и буквы.
    KeySym primary = XK_space;
    KeySym alternate = 0;
    switch (key) {
        case Key::Up:
            primary = XK_Up;
            alternate = XK_w;
            break;
        case Key::Down:
            primary = XK_Down;
            alternate = XK_s;
            break;
        case Key::Left:
            primary = XK_Left;
            alternate = XK_a;
            break;
        case Key::Right:
            primary = XK_Right;
            alternate = XK_d;
            break;
        case Key::Fire:
            break;
    }

    // XQueryKeymap заполняет 32-байтовую карту состояния клавиш X-сервера.
    char keyMap[32] = {}; // XQueryKeymap возвращает 256 бит состояния в 32 байтах.
    XQueryKeymap(display, keyMap);
    // KeyCode — аппаратный код X11; XKeysymToKeycode находит его по символическому KeySym.
    const KeyCode primaryCode = XKeysymToKeycode(display, primary);
    const KeyCode alternateCode = alternate ? XKeysymToKeycode(display, alternate) : 0;

    // Проверяем бит KeyCode: номер байта — code / 8, позиция бита — code % 8.
    const bool primaryPressed = (keyMap[primaryCode / 8] & (1 << (primaryCode % 8))) != 0;
    const bool alternatePressed = alternateCode &&
        (keyMap[alternateCode / 8] & (1 << (alternateCode % 8))) != 0;
    return primaryPressed || alternatePressed;
}


bool present(Error& error)
{
    if (!takeError("before glXSwapBuffers", error)) {
        return false;
    }
    // У GLX нет результата SwapBuffers; ошибки приходят обработчику X11 асинхронно.
    glXSwapBuffers(display, window);
    return takeError("glXSwapBuffers", error);
}

void shutdown(std::vector<Error>& errors)
{
    errors.clear();
    if (display) {
        Error error;
        // Сначала сохраняем отложенную ошибку работы, чтобы не приписать её очистке.
        if (!synchronize("pending X11 requests before shutdown", error)) {
            errors.push_back(error);
        }
        if (openGLContext) {
            const Bool detached = glXMakeCurrent(display, None, nullptr);
            const bool detachSucceeded = synchronize("glXMakeCurrent (detach)", error);
            if (!detachSucceeded) {
                errors.push_back(error);
            } else if (!detached) {
                errors.push_back({ "glXMakeCurrent (detach)", "Could not detach the OpenGL context.", std::nullopt });
            }
            glXDestroyContext(display, openGLContext);
            if (!synchronize("glXDestroyContext", error)) {
                errors.push_back(error);
            }
            openGLContext = nullptr;
        }
        if (window) {
            XDestroyWindow(display, window);
            if (!synchronize("XDestroyWindow", error)) {
                errors.push_back(error);
            }
            window = 0;
        }
        if (colorMap) {
            XFreeColormap(display, colorMap);
            if (!synchronize("XFreeColormap", error)) {
                errors.push_back(error);
            }
            colorMap = 0;
        }
        XCloseDisplay(display);
        display = nullptr;
        // XCloseDisplay тоже может доставить ошибку; после него нельзя вызывать XGetErrorText.
        if (!takeError("XCloseDisplay", error)) {
            errors.push_back(error);
        }
    }
    if (handlersInstalled) {
        XSetErrorHandler(previousErrorHandler);
        XSetIOErrorHandler(previousIOErrorHandler);
        handlersInstalled = false;
    }
    closeMessage = None;
    protocolsMessage = None;
}
}
