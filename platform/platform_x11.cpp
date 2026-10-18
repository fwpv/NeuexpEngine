#include "platform.h"
#include "window_validation.h"

// Xlib управляет окном и событиями, GLX связывает контекст OpenGL с X11.
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
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
bool initialized = false;
bool closeRequested = false;
WindowState windowState;
bool viewportNeedsUpdate = false;
// XQueryKeymap возвращает фиксированные 256 бит состояния клавиатуры.
char keyMap[32] = {};
Atom stateMessage = None;

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

bool requireWindow(const char* operation, Error& error)
{
    if (!initialized || !window) {
        error = { operation, "The window is not initialized or has been destroyed.", std::nullopt };
        return false;
    }
    return true;
}

// В X11 событие PropertyNotify содержит имя свойства, но не его значение.
// Поэтому читаем WM_STATE только при запуске и после уведомления о его изменении.
bool updateMinimizedState(Error& error)
{
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long count = 0;
    unsigned long remaining = 0;
    unsigned char* property = nullptr;
    const int result = XGetWindowProperty(display, window, stateMessage, 0, 2,
        False, stateMessage, &actualType, &actualFormat, &count, &remaining, &property);
    bool minimized = false;
    if (property && actualType == stateMessage && actualFormat == 32 && count >= 1) {
        minimized = reinterpret_cast<const long*>(property)[0] == IconicState;
    }
    if (property) {
        XFree(property);
    }
    if (!takeError("XGetWindowProperty (WM_STATE)", error)) {
        return false;
    }
    if (result != Success) {
        error = { "XGetWindowProperty (WM_STATE)", "Could not read the window state.", result };
        return false;
    }
    windowState.minimized = minimized;
    if (minimized) {
        windowState.focused = false;
    }
    return true;
}

bool readInitialWindowState(Error& error)
{
    XWindowAttributes attributes = {};
    const Status attributesRead = XGetWindowAttributes(display, window, &attributes);
    Window focusedWindow = None;
    int revertMode = 0;
    XGetInputFocus(display, &focusedWindow, &revertMode);
    if (!takeError("readInitialWindowState", error)) {
        return false;
    }
    if (!attributesRead) {
        error = { "XGetWindowAttributes", "Could not read the initial client dimensions.", std::nullopt };
        return false;
    }
    windowState = { attributes.width, attributes.height,
        focusedWindow == window && attributes.map_state == IsViewable, false };
    return updateMinimizedState(error);
}

bool applyWindowTitle(const std::string& title, Error& error)
{
    // Современные менеджеры читают _NET_WM_NAME; WM_NAME оставляем как текстовое свойство UTF-8.
    const Atom utf8 = XInternAtom(display, "UTF8_STRING", False);
    const Atom name = XInternAtom(display, "_NET_WM_NAME", False);
    XChangeProperty(display, window, name, utf8, 8, PropModeReplace,
        reinterpret_cast<const unsigned char*>(title.data()), static_cast<int>(title.size()));
    XChangeProperty(display, window, XA_WM_NAME, utf8, 8, PropModeReplace,
        reinterpret_cast<const unsigned char*>(title.data()), static_cast<int>(title.size()));
    // Смена заголовка редка; подтверждаем результат, чтобы bool имел одинаковый смысл на обеих ОС.
    return synchronize("setWindowTitle", error);
}

bool initializeLinux(const WindowOptions& options, Error& error)
{
    const int width = options.width;
    const int height = options.height;

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
    settings.event_mask = KeyPressMask | StructureNotifyMask | FocusChangeMask | PropertyChangeMask;
    // Маска указывает, какие поля settings используются при создании окна InputOutput.
    // XCreateWindow принимает беззнаковые размеры; положительность проверена до открытия дисплея.
    window = XCreateWindow(display, RootWindow(display, visual->screen), 100, 100,
        static_cast<unsigned int>(width), static_cast<unsigned int>(height),
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

    // WM_NORMAL_HINTS сообщает оконному менеджеру минимальный размер клиентской области.
    // Нули допустимы: движок не подменяет их произвольным положительным минимумом.
    XSizeHints hints = {};
    hints.flags = PMinSize;
    hints.min_width = options.resizable ? options.minimumWidth : options.width;
    hints.min_height = options.resizable ? options.minimumHeight : options.height;
    if (!options.resizable) {
        // Равные минимум и максимум запрещают изменение размера средствами оконного менеджера.
        hints.flags |= PMaxSize;
        hints.max_width = options.width;
        hints.max_height = options.height;
    }
    XSetWMNormalHints(display, window, &hints);
    stateMessage = XInternAtom(display, "WM_STATE", False);
    if (!synchronize("XSetWMNormalHints", error)) {
        XFree(visual);
        return false;
    }

    // False разрешает создавать Atom, если такого имени ещё нет на сервере.
    if (!applyWindowTitle(options.title, error)) {
        XFree(visual);
        return false;
    }
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
    if (!readInitialWindowState(error)) {
        return false;
    }
    viewportNeedsUpdate = true;
    initialized = true;
    return processEvents(error);
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

bool initialize(const WindowOptions& options, std::vector<Error>& errors)
{
    errors.clear();
    if (display || initialized) {
        errors.push_back({ "initialize", "Call shutdown before initializing again.", std::nullopt });
        return false;
    }
    Error error;
    if (!validateWindowOptions(options, error)) {
        errors.push_back(error);
        return false;
    }
    if (initializeLinux(options, error)) {
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
    return requireWindow("setWindowTitle", error)
        && validateWindowTitle(title, "setWindowTitle", error)
        && applyWindowTitle(title, error);
}

bool processEvents(Error& error)
{
    if (initialized && closeRequested && !window) {
        return true;
    }
    if (!requireWindow("processEvents", error)) {
        return false;
    }
    if (!takeError("processEvents", error)) {
        return false;
    }
    bool minimizedStateChanged = false;
    // XPending читает доступные события и ошибки, не ожидая новых событий.
    while (XPending(display)) {
        XEvent event;
        XNextEvent(display, &event);
        if (event.type == MappingNotify) {
            // После смены раскладки Xlib должен обновить соответствие KeySym и KeyCode.
            XRefreshKeyboardMapping(&event.xmapping);
        }
        // События чужих окон не должны менять состояние единственного окна движка.
        if (!window || event.xany.window != window) {
            continue;
        }
        if (event.type == ConfigureNotify) {
            viewportNeedsUpdate = viewportNeedsUpdate || event.xconfigure.width != windowState.width
                || event.xconfigure.height != windowState.height;
            windowState.width = event.xconfigure.width;
            windowState.height = event.xconfigure.height;
        }
        if (event.type == FocusIn || event.type == FocusOut) {
            // Захват мыши/клавиатуры не означает смены фокуса самого окна.
            if (event.xfocus.mode != NotifyGrab && event.xfocus.mode != NotifyUngrab
                && event.xfocus.detail != NotifyInferior) {
                windowState.focused = event.type == FocusIn && !windowState.minimized;
            }
        }
        if (event.type == UnmapNotify) {
            windowState.focused = false;
        }
        if (event.type == MapNotify) {
            windowState.minimized = false;
        }
        if (event.type == PropertyNotify && event.xproperty.atom == stateMessage) {
            minimizedStateChanged = true;
        }
        if (event.type == DestroyNotify && event.xdestroywindow.window == window) {
            windowState.width = 0;
            windowState.height = 0;
            windowState.focused = false;
            windowState.minimized = false;
            window = 0; // Не уничтожаем уже удалённое сервером окно второй раз.
            closeRequested = true;
        }
        if (event.type == ClientMessage && event.xclient.window == window
            && event.xclient.message_type == protocolsMessage && event.xclient.format == 32
            && static_cast<Atom>(event.xclient.data.l[0]) == closeMessage) {
            closeRequested = true;
        }
    }
    // Не запрашиваем свойство уже уничтоженного окна; несколько уведомлений объединяем в одно чтение.
    if (window && minimizedStateChanged && !updateMinimizedState(error)) {
        return false;
    }
    // Область вывода занимает клиентскую область; матрицы игрового мира не меняются.
    if (window && viewportNeedsUpdate) {
        glViewport(0, 0, windowState.width, windowState.height);
        viewportNeedsUpdate = false;
    }
    // Один снимок на обработку событий вместо запроса сервера для каждой клавиши.
    if (window && windowState.focused && !windowState.minimized) {
        XQueryKeymap(display, keyMap);
    }
    // Ошибка имеет приоритет над запросом закрытия, если они пришли в одной очереди.
    return takeError("processEvents", error);
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
    KeySym nativeKey = 0;
    switch (key) {
        case Key::ArrowUp:
            nativeKey = XK_Up;
            break;
        case Key::ArrowDown:
            nativeKey = XK_Down;
            break;
        case Key::ArrowLeft:
            nativeKey = XK_Left;
            break;
        case Key::ArrowRight:
            nativeKey = XK_Right;
            break;
        case Key::W:
            nativeKey = XK_w;
            break;
        case Key::A:
            nativeKey = XK_a;
            break;
        case Key::S:
            nativeKey = XK_s;
            break;
        case Key::D:
            nativeKey = XK_d;
            break;
        case Key::Space:
            nativeKey = XK_space;
            break;
        case Key::Escape:
            nativeKey = XK_Escape;
            break;
        default:
            return false;
    }
    const KeyCode code = XKeysymToKeycode(display, nativeKey);
    if (code == 0) {
        return false;
    }
    return (keyMap[code / 8] & (1 << (code % 8))) != 0;
}


bool present(Error& error)
{
    if (!requireWindow("present", error)) {
        return false;
    }
    if (!takeError("before glXSwapBuffers", error)) {
        return false;
    }
    if (windowState.minimized) {
        return true;
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
            // DestroyNotify может ожидать обработки, если окно удалил другой клиент X11.
            XEvent destroyedEvent;
            if (!XCheckTypedWindowEvent(display, window, DestroyNotify, &destroyedEvent)) {
                const unsigned long destroySerial = NextRequest(display);
                XDestroyWindow(display, window);
                XSync(display, False);
                // Окно могло исчезнуть между проверкой очереди и запросом удаления.
                // Принимаем только BadWindow этого запроса и ресурса с подтверждённым DestroyNotify.
                if (pendingError.error_code == BadWindow && pendingError.resourceid == window
                    && pendingError.serial == destroySerial
                    && XCheckTypedWindowEvent(display, window, DestroyNotify, &destroyedEvent)) {
                    pendingError = {};
                }
                if (!takeError("XDestroyWindow", error)) {
                    errors.push_back(error);
                }
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
    stateMessage = None;
    initialized = false;
    closeRequested = false;
    windowState = {};
    viewportNeedsUpdate = false;
}
}
