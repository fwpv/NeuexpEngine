#include "platform.h"
#include "detail/cleanup_error.h"
#include "window_validation.h"
#include "key_codes.h"
#include "extension_names.h"
#include "detail/platform_input.h"
#include "input.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/Xresource.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <istream>
#include <array>
#include <clocale>
#include <cstring>
#include <string>
#include <utility>
#include <cmath>
#include <locale>
#include <memory>
#include <sstream>
#include <cstddef>
#include <optional>

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
int keyboardEventBase = 0;
unsigned int lockedModifiers = 0;
unsigned int numLockMask = 0;
unsigned int altMask = Mod1Mask;
unsigned int superMask = Mod4Mask;
std::string previousLocale;
std::optional<std::string> previousLocaleModifiers;
Atom stateMessage = None;

bool takeError(const char* operation, Error& error);

Engine::Modifiers readModifiers(unsigned int state)
{
    return {(state & ShiftMask) != 0, (state & ControlMask) != 0,
        (state & altMask) != 0, (state & superMask) != 0,
        (lockedModifiers & LockMask) != 0, (lockedModifiers & numLockMask) != 0};
}

bool changeFocus(bool focused, Error& error)
{
    if (windowState.focused == focused) {
        return true;
    }
    if (focused) {
        // Lock-флаги запрашиваются при получении фокуса; удержание не восстанавливается.
        XkbStateRec state = {};
        const Status result = XkbGetState(display, XkbUseCoreKbd, &state);
        if (!takeError("XkbGetState", error)) {
            return false;
        }
        if (result != Success) {
            error = {"XkbGetState", "Could not read the keyboard lock state.", result};
            return false;
        }
        lockedModifiers = state.locked_mods;
    }
    windowState.focused = focused;
    if (inputContext) {
        if (focused) {
            XSetICFocus(inputContext);
        } else {
            XUnsetICFocus(inputContext);
            // Незавершённая композиция отменяется при потере фокуса.
            char* discarded = Xutf8ResetIC(inputContext);
            if (discarded) {
                XFree(discarded);
            }
        }
    }
    Engine::Event event;
    event.type = focused ? Engine::EventType::FocusGained : Engine::EventType::FocusLost;
    if (focused) {
        event.modifiers = readModifiers(0);
    }
    Detail::queueEvent(event);
    return true;
}

bool readPhysicalKeys(Error& error)
{
    XkbDescPtr keyboard = XkbGetKeyboard(display, XkbAllComponentsMask, XkbUseCoreKbd);
    const bool keyboardSucceeded = takeError("XkbGetKeyboard", error);
    if (!keyboard || !keyboard->names || !keyboard->names->keys || !keyboardSucceeded) {
        if (keyboard) {
            XkbFreeKeyboard(keyboard, XkbAllComponentsMask, True);
        }
        if (keyboardSucceeded) {
            error = {"XkbGetKeyboard", "Physical keyboard names are unavailable.", std::nullopt};
        }
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
        if (physicalKeys[code] == Key::Unknown) {
            // Основное имя клавиши имеет приоритет перед псевдонимами XKB.
            for (unsigned int index = 0; index < keyboard->names->num_key_aliases; ++index) {
                const auto& alias = keyboard->names->key_aliases[index];
                if (std::strncmp(keyboard->names->keys[code].name, alias.real, XkbKeyNameLength) != 0) {
                    continue;
                }
                for (const auto& mapping : Detail::KeyMappings) {
                    if (std::strncmp(alias.alias, mapping.xName, XkbKeyNameLength) == 0) {
                        physicalKeys[code] = mapping.key;
                        break;
                    }
                }
                if (physicalKeys[code] != Key::Unknown) {
                    break;
                }
            }
        }
    }
    XkbFreeKeyboard(keyboard, XkbAllComponentsMask, True);
    numLockMask = XkbKeysymToModifiers(display, XK_Num_Lock);
    altMask = XkbKeysymToModifiers(display, XK_Alt_L) | XkbKeysymToModifiers(display, XK_Alt_R)
        | XkbKeysymToModifiers(display, XK_ISO_Level3_Shift);
    superMask = XkbKeysymToModifiers(display, XK_Super_L) | XkbKeysymToModifiers(display, XK_Super_R);
    return true;
}

void readText(XKeyEvent& keyEvent)
{
    // XBufferOverflow сообщает требуемый размер текста.
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
        // KeyCode 0 — сигнал готового текста от XIM, а не физическая клавиша.
        if (native.xkey.keycode == 0) {
            if (native.type == KeyPress && !filtered) {
                readText(native.xkey);
            }
            return;
        }
        event.key = physicalKeys[native.xkey.keycode & 255U];
        event.modifiers = readModifiers(native.xkey.state);
        // state содержит модификаторы до события; переход применяется отдельно.
        const bool down = native.type == KeyPress;
        if (event.key == Key::LeftShift || event.key == Key::RightShift) {
            event.modifiers.shift = down;
        }
        if (event.key == Key::LeftControl || event.key == Key::RightControl) {
            event.modifiers.control = down;
        }
        if (event.key == Key::LeftAlt || event.key == Key::RightAlt) {
            event.modifiers.alt = down;
        }
        if (event.key == Key::LeftSuper || event.key == Key::RightSuper) {
            event.modifiers.super = down;
        }
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
        // NotifyUngrab возвращает указатель в фактическое окно, в том числе после его открытия под курсором.
        if (native.xcrossing.mode != NotifyNormal && native.xcrossing.mode != NotifyUngrab) {
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
        // XGetErrorText не сообщает требуемый размер буфера.
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
    error = {operation, description + " (X11 request "
        + std::to_string(event.request_code) + "." + std::to_string(event.minor_code)
        + ", serial " + std::to_string(event.serial) + ")", event.error_code};
    return false;
}

bool synchronize(const char* operation, Error& error)
{
    XSync(display, False);
    return takeError(operation, error);
}

bool updateSystemScale(Error& error)
{
    // Xft.dpi — масштаб рабочего стола X11; при отсутствии корректной настройки используется 96 DPI.
    float scale = 1;
    XTextProperty resources = {};
    const Status found = XGetTextProperty(display, DefaultRootWindow(display), &resources, XA_RESOURCE_MANAGER);
    if (found && resources.value && resources.encoding == XA_STRING && resources.format == 8) {
        XrmDatabase database = XrmGetStringDatabase(reinterpret_cast<const char*>(resources.value));
        if (database) {
            char* type = nullptr;
            XrmValue value = {};
            if (XrmGetResource(database, "Xft.dpi", "Xft.Dpi", &type, &value) && value.addr) {
                std::string text(value.addr, value.size);
                if (!text.empty() && text.back() == '\0') {
                    text.pop_back();
                }
                std::istringstream input(text);
                input.imbue(std::locale::classic());
                float dpi = 0;
                if (input >> dpi) {
                    input >> std::ws;
                    const float candidate = dpi / 96;
                    if (input.eof() && std::isfinite(candidate) && candidate > 0) {
                        scale = candidate;
                    }
                }
            }
            XrmDestroyDatabase(database);
        }
    }
    if (resources.value) {
        XFree(resources.value);
    }
    if (!takeError("XGetTextProperty (RESOURCE_MANAGER)", error)) {
        return false;
    }
    if (windowState.systemScale != scale) {
        windowState.systemScale = scale;
        Engine::Event event;
        event.type = Engine::EventType::WindowDpiChanged;
        event.systemScale = scale;
        Detail::queueEvent(event);
    }
    return true;
}

bool requireWindow(const char* operation, Error& error)
{
    if (!initialized || !window) {
        error = {operation, "The window is not initialized or has been destroyed.", std::nullopt};
        return false;
    }
    return true;
}

// WM_STATE читается при запуске и после PropertyNotify этого свойства.
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
        error = {"XGetWindowProperty (WM_STATE)", "Could not read the window state.", result};
        return false;
    }
    if (windowState.minimized != minimized) {
        Detail::queueEvent(minimized ? Engine::EventType::WindowMinimized : Engine::EventType::WindowRestored);
    }
    windowState.minimized = minimized;
    if (minimized && !changeFocus(false, error)) {
        return false;
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
        error = {"XGetWindowAttributes", "Could not read the initial client dimensions.", std::nullopt};
        return false;
    }
    windowState = {attributes.width, attributes.height,
        focusedWindow == window && attributes.map_state == IsViewable, false};
    return updateMinimizedState(error);
}

bool applyWindowTitle(const std::string& title, Error& error)
{
    // Оба свойства заголовка содержат UTF-8.
    const Atom utf8 = XInternAtom(display, "UTF8_STRING", False);
    const Atom name = XInternAtom(display, "_NET_WM_NAME", False);
    XChangeProperty(display, window, name, utf8, 8, PropModeReplace,
        reinterpret_cast<const unsigned char*>(title.data()), static_cast<int>(title.size()));
    XChangeProperty(display, window, XA_WM_NAME, utf8, 8, PropModeReplace,
        reinterpret_cast<const unsigned char*>(title.data()), static_cast<int>(title.size()));
    // Синхронное подтверждение изменения заголовка.
    return synchronize("setWindowTitle", error);
}

bool initializeX11(const WindowOptions& options, Error& error)
{
    Detail::resetInput();
    const int width = options.width;
    const int height = options.height;

    display = XOpenDisplay(nullptr);
    if (!display) {
        error = {"XOpenDisplay", "Could not connect to the X server. Check DISPLAY and access to an X11 or XWayland session.", std::nullopt};
        return false;
    }
    // Глобальные обработчики Xlib восстанавливаются после XCloseDisplay.
    previousErrorHandler = XSetErrorHandler(recordError);
    previousIOErrorHandler = XSetIOErrorHandler(connectionLost);
    handlersInstalled = true;
    pendingError = {};

    XrmInitialize();
    XSelectInput(display, DefaultRootWindow(display), PropertyChangeMask);
    if (!synchronize("XSelectInput (RESOURCE_MANAGER)", error)) {
        return false;
    }

    int attributes[] = {GLX_RGBA, GLX_DOUBLEBUFFER, None};
    std::unique_ptr<XVisualInfo, decltype(&XFree)> visual(
        glXChooseVisual(display, DefaultScreen(display), attributes), XFree);
    const bool visualSucceeded = synchronize("glXChooseVisual", error);
    if (!visual || !visualSucceeded) {
        if (visualSucceeded) {
            error = {"glXChooseVisual", "No compatible OpenGL visual was found.", std::nullopt};
        }
        return false;
    }

    colorMap = XCreateColormap(display, RootWindow(display, visual->screen), visual->visual, AllocNone);
    const bool colorMapSucceeded = synchronize("XCreateColormap", error);
    if (!colorMap || !colorMapSucceeded) {
        colorMap = 0;
        if (colorMapSucceeded) {
            error = {"XCreateColormap", "Could not create the window color map.", std::nullopt};
        }
        return false;
    }

    XSetWindowAttributes settings = {};
    settings.colormap = colorMap;
    settings.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask
        | PointerMotionMask | EnterWindowMask | LeaveWindowMask
        | StructureNotifyMask | FocusChangeMask | PropertyChangeMask;
    window = XCreateWindow(display, RootWindow(display, visual->screen), 100, 100,
        static_cast<unsigned int>(width), static_cast<unsigned int>(height),
        0, visual->depth, InputOutput, visual->visual, CWColormap | CWEventMask, &settings);
    const bool windowSucceeded = synchronize("XCreateWindow", error);
    if (!window || !windowSucceeded) {
        window = 0;
        if (windowSucceeded) {
            error = {"XCreateWindow", "Could not create the application window.", std::nullopt};
        }
        return false;
    }

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
        return false;
    }

    if (!applyWindowTitle(options.title, error)) {
        return false;
    }
    protocolsMessage = XInternAtom(display, "WM_PROTOCOLS", False);
    closeMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
    const Status protocolsSet = XSetWMProtocols(display, window, &closeMessage, 1);
    const bool protocolsSucceeded = synchronize("XSetWMProtocols", error);
    if (!protocolsSet || closeMessage == None || protocolsMessage == None || !protocolsSucceeded) {
        if (protocolsSucceeded) {
            error = {"XSetWMProtocols", "Could not configure the window close protocol.", std::nullopt};
        }
        return false;
    }

    int keyboardOpcode = 0;
    int keyboardErrorBase = 0;
    int keyboardMajor = XkbMajorVersion;
    int keyboardMinor = XkbMinorVersion;
    const Bool keyboardAvailable = XkbQueryExtension(display, &keyboardOpcode, &keyboardEventBase,
        &keyboardErrorBase, &keyboardMajor, &keyboardMinor);
    const bool querySucceeded = takeError("XkbQueryExtension", error);
    if (!keyboardAvailable || !querySucceeded) {
        if (querySucceeded) {
            error = {"XkbQueryExtension", "A compatible XKB extension is unavailable.", std::nullopt};
        }
        return false;
    }
    // XKB доставляет изменения locked_mods отдельными уведомлениями.
    const Bool eventsSelected = XkbSelectEventDetails(display, XkbUseCoreKbd, XkbStateNotify,
        XkbModifierLockMask, XkbModifierLockMask);
    const bool selectionSucceeded = synchronize("XkbSelectEventDetails", error);
    if (!eventsSelected || !selectionSucceeded) {
        if (selectionSucceeded) {
            error = {"XkbSelectEventDetails", "Could not subscribe to keyboard lock state changes.", std::nullopt};
        }
        return false;
    }
    if (!readPhysicalKeys(error)) {
        return false;
    }
    // Detectable autorepeat не создаёт KeyRelease перед повторным KeyPress.
    Bool repeatSupported = False;
    detectableRepeat = XkbSetDetectableAutoRepeat(display, True, &repeatSupported) && repeatSupported;
    const char* oldLocale = std::setlocale(LC_CTYPE, nullptr);
    if (!oldLocale) {
        error = {"setlocale (query LC_CTYPE)", "Could not read the current character locale.", std::nullopt};
        return false;
    }
    previousLocale = oldLocale;
    const char* oldModifiers = XSetLocaleModifiers(nullptr);
    // До первой настройки Xlib модификаторы могут быть ещё не определены.
    if (oldModifiers) {
        previousLocaleModifiers = oldModifiers;
    }
    if (!std::setlocale(LC_CTYPE, "")) {
        error = {"setlocale (LC_CTYPE)", "Could not select the character locale from the environment.", std::nullopt};
        return false;
    }
    if (!XSetLocaleModifiers("")) {
        error = {"XSetLocaleModifiers", "Could not configure the input method from the environment.", std::nullopt};
        return false;
    }
    inputMethod = XOpenIM(display, nullptr, nullptr, nullptr);
    if (!inputMethod) {
        // Локальный XIM обеспечивает Compose/dead keys, если внешний сервер ввода недоступен.
        if (!XSetLocaleModifiers("@im=none")) {
            error = {"XSetLocaleModifiers (@im=none)", "Could not configure the local input method.", std::nullopt};
            return false;
        }
        inputMethod = XOpenIM(display, nullptr, nullptr, nullptr);
    }
    if (inputMethod) {
        inputContext = XCreateIC(inputMethod, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
            XNClientWindow, window, XNFocusWindow, window, nullptr);
    }
    if (!inputContext) {
        error = {"XCreateIC", "Could not create the Unicode input context. Check LC_CTYPE and XIM.", std::nullopt};
        return false;
    }
    // Маска XNFilterEvents дополняет подписку окна для XFilterEvent.
    long filterMask = 0;
    const char* failedAttribute = XGetICValues(inputContext, XNFilterEvents, &filterMask, nullptr);
    if (failedAttribute) {
        error = {"XGetICValues", std::string("Could not read input context attribute: ") + failedAttribute, std::nullopt};
        return false;
    }
    XSelectInput(display, window, settings.event_mask | filterMask);
    if (!synchronize("XSelectInput", error)) {
        return false;
    }

    openGLContext = glXCreateContext(display, visual.get(), nullptr, True);
    visual.reset();
    const bool contextSucceeded = synchronize("glXCreateContext", error);
    if (!openGLContext || !contextSucceeded) {
        openGLContext = nullptr;
        if (contextSucceeded) {
            error = {"glXCreateContext", "Could not create the OpenGL context.", std::nullopt};
        }
        return false;
    }
    const Bool activated = glXMakeCurrent(display, window, openGLContext);
    const bool activationSucceeded = synchronize("glXMakeCurrent", error);
    if (!activated || !activationSucceeded) {
        if (activationSucceeded) {
            error = {"glXMakeCurrent", "Could not activate the OpenGL context.", std::nullopt};
        }
        return false;
    }
    // Окно передаётся оконному менеджеру только после подготовки XIM и текущего GL-контекста.
    XMapWindow(display, window);
    if (!synchronize("XMapWindow", error)) {
        return false;
    }
    if (!readInitialWindowState(error) || !updateSystemScale(error)) {
        return false;
    }
    if (windowState.focused) {
        windowState.focused = false;
        if (!changeFocus(true, error)) {
            return false;
        }
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

bool initialize(const WindowOptions& options, Error& error)
{
    error = {};
    if (display || initialized || !previousLocale.empty()) {
        error = Error{"initialize", "Call shutdown before initializing again.", std::nullopt};
        return false;
    }
    if (!validateWindowOptions(options, error)) {
        return false;
    }
    Detail::StartupRollback rollback;
    rollback.committed = initializeX11(options, error);
    return rollback.committed;
}

bool getWindowState(WindowState& state, Error& error)
{
    if (!initialized) {
        error = {"getWindowState", "The platform is not initialized.", std::nullopt};
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

bool setVSync(bool enabled, Error& error)
{
    if (!requireWindow("setVSync", error) || !takeError("before setVSync", error)) {
        return false;
    }
    const char* extensions = glXQueryExtensionsString(display, DefaultScreen(display));
    if (!synchronize("glXQueryExtensionsString", error)) {
        return false;
    }
    if (Detail::hasExtension(extensions, "GLX_EXT_swap_control")) {
        using SwapInterval = void (*)(Display*, GLXDrawable, int);
        const auto swapInterval = reinterpret_cast<SwapInterval>(
            glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glXSwapIntervalEXT")));
        if (swapInterval) {
            swapInterval(display, window, enabled ? 1 : 0);
            return synchronize("glXSwapIntervalEXT", error);
        }
    }
    if (Detail::hasExtension(extensions, "GLX_MESA_swap_control")) {
        const Bool direct = glXIsDirect(display, openGLContext);
        if (!synchronize("glXIsDirect", error)) {
            return false;
        }
        if (direct) {
            using SwapInterval = int (*)(unsigned int);
            const auto swapInterval = reinterpret_cast<SwapInterval>(
                glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glXSwapIntervalMESA")));
            if (swapInterval) {
                const int result = swapInterval(enabled ? 1U : 0U);
                if (!synchronize("glXSwapIntervalMESA", error)) {
                    return false;
                }
                if (result != 0) {
                    error = {"glXSwapIntervalMESA", "Could not change VSync.", result};
                    return false;
                }
                return true;
            }
        }
    }
    if (enabled && Detail::hasExtension(extensions, "GLX_SGI_swap_control")) {
        using SwapInterval = int (*)(int);
        const auto swapInterval = reinterpret_cast<SwapInterval>(
            glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glXSwapIntervalSGI")));
        if (swapInterval) {
            const int result = swapInterval(1);
            if (!synchronize("glXSwapIntervalSGI", error)) {
                return false;
            }
            if (result != 0) {
                error = {"glXSwapIntervalSGI", "Could not enable VSync.", result};
                return false;
            }
            return true;
        }
    }
    error = {"setVSync", enabled ? "VSync control is unavailable." :
        "Disabling VSync requires GLX_EXT_swap_control or GLX_MESA_swap_control.", std::nullopt};
    return false;
}

bool processEvents(Error& error)
{
    // Полученные события публикуются и при ошибке обработки.
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
        // События расширения имеют другую структуру: их нельзя проверять через xany.window.
        if (event.type == keyboardEventBase) {
            const auto& keyboardEvent = reinterpret_cast<const XkbEvent&>(event);
            if (keyboardEvent.any.xkb_type == XkbStateNotify) {
                lockedModifiers = keyboardEvent.state.locked_mods;
                if (windowState.focused) {
                    Engine::Event changed;
                    changed.type = Engine::EventType::LockStateChanged;
                    changed.modifiers = readModifiers(0);
                    Detail::queueEvent(changed);
                }
            }
            continue;
        }
        // В старом autorepeat искусственный Release/Press имеют одинаковое время.
        if (!detectableRepeat && event.type == KeyRelease && XPending(display)) {
            XEvent next;
            XPeekEvent(display, &next);
            if (next.type == KeyPress && next.xkey.window == event.xkey.window
                && next.xkey.keycode == event.xkey.keycode && next.xkey.time == event.xkey.time) {
                continue;
            }
        }
        const bool filtered = XFilterEvent(&event, None) != False;
        if (window && event.type == PropertyNotify && event.xproperty.window == DefaultRootWindow(display)
            && event.xproperty.atom == XA_RESOURCE_MANAGER) {
            if (!updateSystemScale(error)) {
                return false;
            }
            continue;
        }
        if (event.type == MappingNotify) {
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
                if (!changeFocus(event.type == FocusIn && !windowState.minimized, error)) {
                    return false;
                }
            }
        }
        if (event.type == UnmapNotify && !changeFocus(false, error)) {
            return false;
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
            if (!changeFocus(false, error)) {
                return false;
            }
            Detail::queueEvent(Engine::EventType::WindowDestroyed);
            windowState = {};
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
    // Уведомления WM_STATE объединяются в одно чтение для существующего окна.
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

bool shutdown(Error& result)
{
    result = {};
    if (display) {
        Error error;
        // Отложенная ошибка работы сохраняется отдельно от ошибок очистки.
        if (!synchronize("pending X11 requests before shutdown", error)) {
            Detail::keepFirstError(result, error);
        }
        if (inputContext) {
            XDestroyIC(inputContext);
            inputContext = nullptr;
            if (!synchronize("XDestroyIC", error)) {
                Detail::keepFirstError(result, error);
            }
        }
        if (inputMethod) {
            const Status closed = XCloseIM(inputMethod);
            inputMethod = nullptr;
            if (!synchronize("XCloseIM", error)) {
                Detail::keepFirstError(result, error);
            } else if (!closed) {
                Detail::keepFirstError(result, Error{"XCloseIM", "Could not close the input method.", std::nullopt});
            }
        }
        if (openGLContext) {
            const Bool detached = glXMakeCurrent(display, None, nullptr);
            const bool detachSucceeded = synchronize("glXMakeCurrent (detach)", error);
            if (!detachSucceeded) {
                Detail::keepFirstError(result, error);
            } else if (!detached) {
                Detail::keepFirstError(result, Error{"glXMakeCurrent (detach)", "Could not detach the OpenGL context.", std::nullopt});
            }
            glXDestroyContext(display, openGLContext);
            if (!synchronize("glXDestroyContext", error)) {
                Detail::keepFirstError(result, error);
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
                    Detail::keepFirstError(result, error);
                }
            }
            window = 0;
        }
        if (colorMap) {
            XFreeColormap(display, colorMap);
            if (!synchronize("XFreeColormap", error)) {
                Detail::keepFirstError(result, error);
            }
            colorMap = 0;
        }
        XCloseDisplay(display);
        display = nullptr;
        // XCloseDisplay тоже может доставить ошибку; после него нельзя вызывать XGetErrorText.
        if (!takeError("XCloseDisplay", error)) {
            Detail::keepFirstError(result, error);
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
    keyboardEventBase = 0;
    lockedModifiers = 0;
    numLockMask = 0;
    altMask = Mod1Mask;
    superMask = Mod4Mask;
    // При отказе сохраняем настройки для повторного shutdown().
    if (!previousLocale.empty()) {
        if (!std::setlocale(LC_CTYPE, previousLocale.c_str())) {
            Detail::keepFirstError(result, Error{"setlocale (restore LC_CTYPE)",
                "Could not restore the previous character locale.", std::nullopt});
        } else if (previousLocaleModifiers && !XSetLocaleModifiers(previousLocaleModifiers->c_str())) {
            Detail::keepFirstError(result, Error{"XSetLocaleModifiers (restore)",
                "Could not restore the previous locale modifiers.", std::nullopt});
        } else {
            previousLocale.clear();
            previousLocaleModifiers.reset();
        }
    }
    Detail::resetInput();
    return result.operation.empty();
}
}
