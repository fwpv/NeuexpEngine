#include "platform.h"

// Xlib отвечает за соединение с X-сервером, окна и события; keysym переводит
// коды клавиш в символы; gl.h содержит команды OpenGL, а glx.h связывает OpenGL с X11.
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>

namespace Platform
{
namespace
{
// Display* — соединение с X-сервером; Window, Colormap и Atom — X11-идентификаторы;
// GLXContext — контекст OpenGL для X11. Все они остаются внутри платформенного модуля.
Display* display = nullptr;
Window window = 0;
Colormap colorMap = 0;
GLXContext openGLContext = nullptr;
Atom closeMessage = 0;

bool initializeLinux(int width, int height)
{
    // XOpenDisplay открывает соединение с X-сервером; nullptr просит использовать DISPLAY по умолчанию.
    display = XOpenDisplay(nullptr);
    if (!display) {
        return false;
    }

    // XVisualInfo описывает формат пикселей и глубину окна; glXChooseVisual выбирает
    // подходящий вариант, поддерживаемый X-сервером и GLX.
    // GLX_RGBA просит RGBA-цвет, GLX_DOUBLEBUFFER — два буфера,
    // GLX_DEPTH_SIZE просит размер буфера глубины 24; None завершает список атрибутов.
    int attributes[] = {
        GLX_RGBA,          // Требовать визуал с цветовыми компонентами RGBA.
        GLX_DOUBLEBUFFER,  // Требовать передний и задний буферы для плавного показа кадров.
        GLX_DEPTH_SIZE, 24, // Минимальный размер буфера глубины — 24 бита.
        None               // Маркер конца списка атрибутов GLX.
    };

    // DefaultScreen возвращает экран X11 по умолчанию.
    XVisualInfo* visual = glXChooseVisual(display, DefaultScreen(display), attributes);
    if (!visual) {
        return false;
    }

    // RootWindow возвращает корневое окно.
    // XCreateColormap создаёт цветовую карту для visual; AllocNone не выделяет ячейки заранее.
    colorMap = XCreateColormap(display, RootWindow(display, visual->screen), visual->visual, AllocNone);

    // XSetWindowAttributes хранит параметры нового окна.
    // KeyPressMask подписывает на нажатия клавиш, StructureNotifyMask — на изменение/уничтожение окна.
    XSetWindowAttributes settings = {};
    settings.colormap = colorMap; // Цветовая карта, совместимая с выбранным visual.
    settings.event_mask = KeyPressMask | StructureNotifyMask;
    // colormap и event_mask — поля XSetWindowAttributes, подключённые маской ниже.

    // XCreateWindow создаёт окно InputOutput (обычное рисуемое окно) с глубиной visual->depth.
    // Нулевой border_width означает отсутствие дополнительной границы X11;
    // visual->visual задаёт выбранный формат изображения окна.
    // x/y = 100 задают начальное положение; width/height переданы из настроек мира.
    // CWColormap | CWEventMask указывает, что X11 должен взять эти два значения из settings.
    window = XCreateWindow(display, RootWindow(display, visual->screen), 100, 100, width, height,
        0, visual->depth, InputOutput, visual->visual, CWColormap | CWEventMask, &settings);
    if (!window) {
        XFree(visual); // XFree освобождает память XVisualInfo.
        return false;
    }

    // XStoreName устанавливает заголовок. XInternAtom получает Atom — уникальный X11-идентификатор
    // имени WM_DELETE_WINDOW; False запрещает создавать Atom, если оконный менеджер его не знает.
    XStoreName(display, window, "ORBITAL // SECTOR 09"); // Имя свойства заголовка окна.
    closeMessage = XInternAtom(display, "WM_DELETE_WINDOW", False); // False: не создавать неизвестный Atom.

    // XSetWMProtocols регистрирует WM_DELETE_WINDOW как поддерживаемый протокол закрытия окна.
    XSetWMProtocols(display, window, &closeMessage, 1); // Последний аргумент — число Atom в списке.

    // XMapWindow делает окно отображаемым; оконный менеджер затем выбирает его положение.
    XMapWindow(display, window);

    // glXCreateContext создаёт GLXContext с тем же XVisualInfo, что и окно.
    // Аргумент True просит прямой рендеринг через графический драйвер, если он доступен.
    openGLContext = glXCreateContext(display, visual, nullptr, True);

    // После создания контекста информация о визуале больше не нужна; XFree освобождает её память.
    XFree(visual);

    // glXMakeCurrent связывает Display, Window и GLXContext с текущим потоком;
    // после этого команды gl* направляются в данное окно.
    // При ошибке создания или привязки возвращаем false; вызывающая сторона вызовет shutdown.
    if (!openGLContext || !glXMakeCurrent(display, window, openGLContext)) {
        return false;
    }
    return true;
}
}

// Общий интерфейс перенаправляет инициализацию в реализацию X11/GLX.
bool initialize(int width, int height)
{
    return initializeLinux(width, height);
}

void setWindowTitle(const char* title)
{
    // XStoreName меняет свойство заголовка; XFlush немедленно отправляет буфер запросов X-серверу.
    if (display && window) {
        XStoreName(display, window, title);
        XFlush(display);
    }
}

bool processEvents()
{
    // XPending возвращает число ожидающих событий и не блокирует цикл, если очередь пуста.
    while (XPending(display)) {
        // XEvent — объединённая структура разных типов событий; XNextEvent забирает следующее событие.
        XEvent event;
        XNextEvent(display, &event);
        // DestroyNotify — событие X11 об уничтожении окна.
        if (event.type == DestroyNotify) {
            return false;
        }

        // ClientMessage — сообщение оконного менеджера; xclient.data.l[0] содержит Atom протокола.
        // Сравниваем его с зарегистрированным выше WM_DELETE_WINDOW.
        if (event.type == ClientMessage && static_cast<Atom>(event.xclient.data.l[0]) == closeMessage) {
            return false;
        }

        // KeyPress — событие нажатия; XLookupKeysym переводит его KeyCode в KeySym.
        // Его индекс 0 выбирает первый, обычно не сдвинутый, символ клавиши.
        // XK_Escape — KeySym клавиши Escape.
        if (event.type == KeyPress && XLookupKeysym(&event.xkey, 0) == XK_Escape) {
            return false;
        }
    }
    return true;
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

void present()
{
    // glXSwapBuffers меняет задний и передний буферы drawable-окна, показывая готовый кадр.
    glXSwapBuffers(display, window);
}

void shutdown()
{
    // glXMakeCurrent с None и nullptr отвязывает контекст от drawable и потока;
    // glXDestroyContext удаляет GLXContext.
    if (openGLContext) {
        glXMakeCurrent(display, None, nullptr);
        glXDestroyContext(display, openGLContext);
        openGLContext = nullptr;
    }

    // XDestroyWindow удаляет Window; XFreeColormap освобождает Colormap;
    // XCloseDisplay закрывает соединение и освобождает оставшиеся ресурсы Xlib.
    if (display) {
        if (window) {
            XDestroyWindow(display, window);
        }
        if (colorMap) {
            XFreeColormap(display, colorMap);
        }
        XCloseDisplay(display);
        display = nullptr;
        window = 0;
        colorMap = 0;
    }
}
}
