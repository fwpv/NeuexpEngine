#include "platform.h"
#include "window_validation.h"
#include "key_codes.h"
#include "detail/platform_input.h"

// Xlib управляет окном и событиями, GLX связывает контекст OpenGL с X11.
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <array>
#include <clocale>
#include <cstring>
#include <string>
#include <utility>

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
std::array<Key, 256> physicalKeys = {};
XIM inputMethod = nullptr;
XIC inputContext = nullptr;
bool detectableRepeat = false;
unsigned int numLockMask = 0;
unsigned int altMask = Mod1Mask;
unsigned int superMask = Mod4Mask;
std::string previousLocale;
std::string previousLocaleModifiers;
Atom stateMessage = None;

Engine::Modifiers readModifiers(unsigned int state)
{
    return { (state & ShiftMask) != 0, (state & ControlMask) != 0,
        (state & altMask) != 0, (state & superMask) != 0,
        (state & LockMask) != 0, (state & numLockMask) != 0 };
}

void changeFocus(bool focused)
{
    if (windowState.focused == focused) {
        return;
    }
    windowState.focused = focused;
    if (inputContext) {
        if (focused) {
            XSetICFocus(inputContext);
        } else {
            XUnsetICFocus(inputContext);
            // Отменяем незавершённую композицию при уходе из окна.
            char* discarded = Xutf8ResetIC(inputContext);
            if (discarded) {
                XFree(discarded);
            }
        }
    }
    Engine::Event event;
    event.type = focused ? Engine::EventType::FocusGained : Engine::EventType::FocusLost;
    if (focused) {
        // FocusIn не содержит lock-флагов. Читаем их один раз при входе, не восстанавливая
        // удержание клавиш из снимка и не опрашивая сервер на каждом кадре.
        // XkbUseCoreKbd выбирает основную клавиатуру; Success подтверждает получение
        // XkbStateRec. locked_mods содержит переключатели, а не удерживаемые модификаторы.
        XkbStateRec state = {};
        if (XkbGetState(display, XkbUseCoreKbd, &state) == Success) {
            event.modifiers.capsLock = (state.locked_mods & LockMask) != 0;
            event.modifiers.numLock = (state.locked_mods & numLockMask) != 0;
        }
    }
    Detail::queueEvent(event);
}

bool readPhysicalKeys(Error& error)
{
    // XkbGetKeyboard возвращает описание основной клавиатуры. XkbAllComponentsMask
    // запрашивает в том числе имена физических клавиш; nullptr означает отказ.
    // Описание принадлежит вызывающему коду и освобождается XkbFreeKeyboard.
    XkbDescPtr keyboard = XkbGetKeyboard(display, XkbAllComponentsMask, XkbUseCoreKbd);
    if (!keyboard || !keyboard->names || !keyboard->names->keys) {
        if (keyboard) {
            XkbFreeKeyboard(keyboard, XkbAllComponentsMask, True);
        }
        error = { "XkbGetKeyboard", "Physical keyboard names are unavailable.", std::nullopt };
        return false;
    }
    physicalKeys.fill(Key::Unknown);
    for (unsigned int code = keyboard->min_key_code; code <= keyboard->max_key_code; ++code) {
        for (const auto& mapping : Detail::KeyMappings) {
            // XKB хранит имя в четырёх байтах без обязательного завершающего нуля.
            if (std::strncmp(keyboard->names->keys[code].name, mapping.xName, XkbKeyNameLength) == 0) {
                physicalKeys[code] = mapping.key;
                break;
            }
        }
    }
    XkbFreeKeyboard(keyboard, XkbAllComponentsMask, True);
    // XkbKeysymToModifiers переводит символ в маску модификатора текущей карты.
    // Ноль означает, что символ не назначен модификатору; AltGr также учитывается как Alt.
    numLockMask = XkbKeysymToModifiers(display, XK_Num_Lock);
    altMask = XkbKeysymToModifiers(display, XK_Alt_L) | XkbKeysymToModifiers(display, XK_Alt_R)
        | XkbKeysymToModifiers(display, XK_ISO_Level3_Shift);
    superMask = XkbKeysymToModifiers(display, XK_Super_L) | XkbKeysymToModifiers(display, XK_Super_R);
    return true;
}

void readText(XKeyEvent& keyEvent)
{
    // Xutf8LookupString поддерживает раскладки, dead keys и подтверждение через XIM.
    // При переполнении Xlib сообщает точный размер; фиксированного ограничения текста нет.
    std::string text(32, '\0');
    Status status = 0;
    KeySym symbol = NoSymbol;
    int length = Xutf8LookupString(inputContext, &keyEvent, text.data(),
        static_cast<int>(text.size()), &symbol, &status);
    if (status == XBufferOverflow) {
        text.resize(static_cast<std::size_t>(length));
        length = Xutf8LookupString(inputContext, &keyEvent, text.data(), length, &symbol, &status);
    }
    if ((status == XLookupChars || status == XLookupBoth) && length > 0) {
        text.resize(static_cast<std::size_t>(length));
        // Управляющие ASCII приходят через события клавиш; UTF-8 байты сохраняются без изменений.
        if (static_cast<unsigned char>(text.front()) >= 0x20 && text.front() != '\x7f') {
            Engine::Event event;
            event.type = Engine::EventType::TextInput;
            event.text = std::move(text);
            event.modifiers = readModifiers(keyEvent.state);
            Detail::queueEvent(event);
        }
    }
}

void handleInput(XEvent& native, bool filtered)
{
    using Engine::EventType;
    using Engine::MouseButton;
    Engine::Event event;
    if (native.type == KeyPress || native.type == KeyRelease) {
        if (!windowState.focused) {
            return;
        }
        event.type = native.type == KeyPress ? EventType::KeyPressed : EventType::KeyReleased;
        event.key = physicalKeys[native.xkey.keycode & 255U];
        event.modifiers = readModifiers(native.xkey.state);
        // state относится к состоянию ДО события. Для модификатора добавляем сам переход.
        const bool down = native.type == KeyPress;
        if (event.key == Key::LeftShift || event.key == Key::RightShift) { event.modifiers.shift = down; }
        if (event.key == Key::LeftControl || event.key == Key::RightControl) { event.modifiers.control = down; }
        if (event.key == Key::LeftAlt || event.key == Key::RightAlt) { event.modifiers.alt = down; }
        if (event.key == Key::LeftSuper || event.key == Key::RightSuper) { event.modifiers.super = down; }
        if (down && event.key == Key::CapsLock) { event.modifiers.capsLock = !event.modifiers.capsLock; }
        if (down && event.key == Key::NumLock) { event.modifiers.numLock = !event.modifiers.numLock; }
        Detail::queueEvent(event);
        if (down && !filtered) {
            readText(native.xkey);
        }
        return;
    }
    if (filtered) {
        return;
    }
    if (native.type == MotionNotify) {
        event.type = EventType::MouseMoved;
        event.x = native.xmotion.x;
        event.y = native.xmotion.y;
        event.modifiers = readModifiers(native.xmotion.state);
    } else if (native.type == EnterNotify || native.type == LeaveNotify) {
        event.type = native.type == EnterNotify ? EventType::MouseEntered : EventType::MouseLeft;
        event.x = native.xcrossing.x;
        event.y = native.xcrossing.y;
        event.modifiers = readModifiers(native.xcrossing.state);
        // NotifyGrab/NotifyUngrab не являются физическим пересечением границы окна.
        if (native.xcrossing.mode != NotifyNormal) {
            return;
        }
    } else if (native.type == ButtonPress || native.type == ButtonRelease) {
        event.x = native.xbutton.x;
        event.y = native.xbutton.y;
        event.modifiers = readModifiers(native.xbutton.state);
        const unsigned int button = native.xbutton.button;
        if (button >= 4 && button <= 7) {
            // В core X11 колесо представлено парами кнопок; отпускание не означает второй шаг.
            if (native.type == ButtonRelease) {
                return;
            }
            event.type = EventType::MouseWheel;
            event.wheelY = button == 4 ? 1 : button == 5 ? -1 : 0;
            event.wheelX = button == 7 ? 1 : button == 6 ? -1 : 0;
        } else {
            if (!windowState.focused) {
                return;
            }
            event.type = native.type == ButtonPress ? EventType::MousePressed : EventType::MouseReleased;
            switch (button) {
                case 1: event.button = MouseButton::Left; break;
                case 2: event.button = MouseButton::Middle; break;
                case 3: event.button = MouseButton::Right; break;
                case 8: event.button = MouseButton::Back; break;
                case 9: event.button = MouseButton::Forward; break;
                default: event.button = MouseButton::Unknown; break;
            }
        }
    } else {
        // Остальные события обслуживаются оконной частью processEvents и фильтром XIM.
        return;
    }
    Detail::queueEvent(event);
}

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
    if (windowState.minimized != minimized) {
        Detail::queueEvent(minimized ? Engine::EventType::WindowMinimized : Engine::EventType::WindowRestored);
    }
    windowState.minimized = minimized;
    if (minimized) {
        changeFocus(false);
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
    Detail::resetInput();
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
    settings.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask
        | PointerMotionMask | EnterWindowMask | LeaveWindowMask
        | StructureNotifyMask | FocusChangeMask | PropertyChangeMask;
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

    if (!readPhysicalKeys(error)) {
        XFree(visual);
        return false;
    }
    // Detectable autorepeat оставляет только повторные KeyPress, без искусственного
    // KeyRelease перед каждым повтором. True запрашивает режим для этого соединения,
    // repeatSupported сообщает поддержку сервером; иначе processEvents разбирает пары вручную.
    Bool repeatSupported = False;
    detectableRepeat = XkbSetDetectableAutoRepeat(display, True, &repeatSupported) && repeatSupported;
    // LC_CTYPE определяет правила ввода символов. Пустая строка выбирает локаль окружения,
    // nullptr читает текущую; копия нужна для восстановления после завершения платформы.
    previousLocale = std::setlocale(LC_CTYPE, nullptr);
    // XSetLocaleModifiers задаёт настройки XIM: nullptr читает текущие модификаторы,
    // пустая строка выбирает XMODIFIERS из окружения, например сервер IBus/Fcitx.
    const char* oldModifiers = XSetLocaleModifiers(nullptr);
    previousLocaleModifiers = oldModifiers ? oldModifiers : "";
    std::setlocale(LC_CTYPE, "");
    XSetLocaleModifiers("");
    // XOpenIM открывает метод ввода (XIM), связанный с display. Остальные nullptr
    // отключают базу ресурсов и имена приложения для неё. При отказе возвращается nullptr.
    inputMethod = XOpenIM(display, nullptr, nullptr, nullptr);
    if (!inputMethod) {
        // Локальный XIM обеспечивает Compose/dead keys, если внешний сервер ввода недоступен.
        XSetLocaleModifiers("@im=none");
        inputMethod = XOpenIM(display, nullptr, nullptr, nullptr);
    }
    if (inputMethod) {
        // XCreateIC создаёт контекст ввода (XIC) для окна; аргументы идут парами имя/значение.
        // XNInputStyle задаёт способ отображения незавершённой композиции и статуса:
        // XIMPreeditNothing/XIMStatusNothing оставляют их отображение методу ввода.
        // XNClientWindow — окно-владелец контекста, XNFocusWindow — получатель ввода.
        // nullptr завершает список. При отказе возвращается nullptr; успешный контекст
        // освобождается XDestroyIC до XCloseIM, закрывающего сам метод ввода.
        inputContext = XCreateIC(inputMethod, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
            XNClientWindow, window, XNFocusWindow, window, nullptr);
    }
    if (!inputContext) {
        error = { "XCreateIC", "Could not create the Unicode input context. Check LC_CTYPE and XIM.", std::nullopt };
        XFree(visual);
        return false;
    }
    // XNFilterEvents сообщает маску событий, нужных методу ввода. Добавляем её к событиям
    // приложения: XFilterEvent должен получать эти сообщения для обработки композиции.
    long filterMask = 0;
    XGetICValues(inputContext, XNFilterEvents, &filterMask, nullptr);
    XSelectInput(display, window, settings.event_mask | filterMask);
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
    if (windowState.focused) {
        windowState.focused = false;
        changeFocus(true);
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
    // Публикуем и при ошибке уже полученные события; следующий вызов начинает новый кадр.
    struct FramePublisher
    {
        ~FramePublisher()
        {
            Detail::finishInputFrame();
        }
    } publisher;
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
        // Старые серверы имитируют autorepeat парой Release/Press с одним временем.
        // Убираем только искусственный Release; следующий Press сохраняется как repeat.
        if (!detectableRepeat && event.type == KeyRelease && XPending(display)) {
            XEvent next;
            XPeekEvent(display, &next);
            if (next.type == KeyPress && next.xkey.window == event.xkey.window
                && next.xkey.keycode == event.xkey.keycode && next.xkey.time == event.xkey.time) {
                continue;
            }
        }
        const bool filtered = XFilterEvent(&event, None) != False;
        if (event.type == MappingNotify) {
            // После смены раскладки Xlib должен обновить соответствие KeySym и KeyCode.
            XRefreshKeyboardMapping(&event.xmapping);
            if (!readPhysicalKeys(error)) {
                return false;
            }
        }
        // События чужих окон не должны менять состояние единственного окна движка.
        if (!window || event.xany.window != window) {
            continue;
        }
        handleInput(event, filtered);
        if (event.type == ConfigureNotify) {
            viewportNeedsUpdate = viewportNeedsUpdate || event.xconfigure.width != windowState.width
                || event.xconfigure.height != windowState.height;
            if (windowState.width != event.xconfigure.width || windowState.height != event.xconfigure.height) {
                Engine::Event resized;
                resized.type = Engine::EventType::WindowResized;
                resized.width = event.xconfigure.width;
                resized.height = event.xconfigure.height;
                Detail::queueEvent(resized);
            }
            windowState.width = event.xconfigure.width;
            windowState.height = event.xconfigure.height;
        }
        if (event.type == FocusIn || event.type == FocusOut) {
            // Захват мыши/клавиатуры не означает смены фокуса самого окна.
            if (event.xfocus.mode != NotifyGrab && event.xfocus.mode != NotifyUngrab
                && event.xfocus.detail != NotifyInferior) {
                changeFocus(event.type == FocusIn && !windowState.minimized);
            }
        }
        if (event.type == UnmapNotify) {
            changeFocus(false);
        }
        if (event.type == MapNotify) {
            if (windowState.minimized) {
                Detail::queueEvent(Engine::EventType::WindowRestored);
            }
            windowState.minimized = false;
        }
        if (event.type == PropertyNotify && event.xproperty.atom == stateMessage) {
            minimizedStateChanged = true;
        }
        if (event.type == DestroyNotify && event.xdestroywindow.window == window) {
            changeFocus(false);
            Detail::queueEvent(Engine::EventType::WindowDestroyed);
            windowState.width = 0;
            windowState.height = 0;
            windowState.focused = false;
            windowState.minimized = false;
            window = 0; // Не уничтожаем уже удалённое сервером окно второй раз.
            if (!closeRequested) {
                Detail::queueEvent(Engine::EventType::CloseRequested);
            }
            closeRequested = true;
        }
        if (event.type == ClientMessage && event.xclient.window == window
            && event.xclient.message_type == protocolsMessage && event.xclient.format == 32
            && static_cast<Atom>(event.xclient.data.l[0]) == closeMessage) {
            closeRequested = true;
            Detail::queueEvent(Engine::EventType::CloseRequested);
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
    // Ошибка имеет приоритет над запросом закрытия, если они пришли в одной очереди.
    return takeError("processEvents", error);
}

bool isCloseRequested()
{
    return initialized && closeRequested;
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
        if (inputContext) {
            XDestroyIC(inputContext);
            inputContext = nullptr;
        }
        if (inputMethod) {
            XCloseIM(inputMethod);
            inputMethod = nullptr;
        }
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
    physicalKeys.fill(Key::Unknown);
    detectableRepeat = false;
    numLockMask = 0;
    altMask = Mod1Mask;
    superMask = Mod4Mask;
    if (!previousLocale.empty()) {
        std::setlocale(LC_CTYPE, previousLocale.c_str());
        previousLocale.clear();
        XSetLocaleModifiers(previousLocaleModifiers.c_str());
        previousLocaleModifiers.clear();
    }
    Detail::resetInput();
}
}
