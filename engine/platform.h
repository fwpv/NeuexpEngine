#pragma once

#include <optional>
#include <string>
#include <vector>

// Единая граница между игровой логикой и операционной системой.
// Игровой код вызывает эти функции, а CMake подключает реализацию
// только для выбранной платформы: Windows или Linux/X11.
namespace Platform
{
// Данные ошибки не зависят от Win32/X11. Наличие ошибки определяется результатом операции.
struct Error
{
    std::string operation;
    std::string message;
    std::optional<int> nativeCode;
};

enum class EventResult
{
    Continue,
    Quit,
    Failed
};

// Всегда пишет в stderr; в Windows дополнительно показывает MessageBox, если showDialog == true.
// Для некритичных ошибок и ошибок очистки диалог можно отключить.
void reportError(const Error& error, bool showDialog = true);

// Игровые действия перечислены независимо от кодов клавиш конкретной ОС.
enum class Key
{
    // Направления движения; реализации связывают их со стрелками и WASD.
    Up,
    Down,
    Left,
    Right,
    // Огневая кнопка; реализации назначают ей пробел.
    Fire
};

struct WindowOptions
{
    int width = 800;
    int height = 600;
    // Ноль не добавляет ограничений движка к ограничениям оконной системы.
    int minimumWidth = 0;
    int minimumHeight = 0;
    // false запрещает пользователю менять размер окна и разворачивать его на весь экран.
    bool resizable = true;
};

struct WindowState
{
    int width = 0;
    int height = 0;
    bool focused = false;
    bool minimized = false;
};

// Создаёт окно и контекст OpenGL. Размеры клиентской области задаются в пикселях.
// При false заполняет error; частично созданные ресурсы освобождаются через shutdown().
// Начальные размеры положительные, минимальные — неотрицательные и не больше начальных.
// Общий предел 65535 пикселей соответствует размерным полям протокола X11.
// Повторная инициализация запрещена до shutdown(), включая частично неудачный запуск.
bool initialize(const WindowOptions& options, Error& error);

// Возвращает сохранённые размеры клиентской области и состояние окна без обращений к ОС.
// Начальный снимок создаётся при initialize(), затем обновляется обработчиками событий ОС
// (обычно во время processEvents()). До обработки новых событий возвращается прежний снимок.
// При ошибке state не меняется; после уничтожения окна возвращается пустой снимок до shutdown().
bool getWindowState(WindowState& state, Error& error);

// Меняет системный заголовок окна, например, чтобы показывать текущий FPS.
bool setWindowTitle(const char* title, Error& error);

// Отличает нормальное закрытие от ошибки; error читается только при Failed.
EventResult processEvents(Error& error);

// Отдельно хранит запрос выхода, обнаруженный при обработке событий.
// Чтение не обрабатывает события. До инициализации и после shutdown() возвращает false.
// Обнаруженный запрос сохраняется до shutdown(), даже при последующей ошибке обработки событий.
bool isCloseRequested();

// Проверяет, нажата ли клавиша, соответствующая игровому действию.
bool isKeyDown(Key key);

// Показывает нарисованный кадр из заднего буфера окна.
bool present(Error& error);

// Освобождает созданные ресурсы ОС и графической подсистемы.
// Продолжает очистку после отказа отдельной операции; заменяет errors списком ошибок этого вызова.
void shutdown(std::vector<Error>& errors);
}
