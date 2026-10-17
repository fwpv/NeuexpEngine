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

// Создаёт окно и связывает его с контекстом OpenGL. Размеры задаются в пикселях.
// При false заполняет error; частично созданные ресурсы освобождаются через shutdown().
bool initialize(int width, int height, Error& error);

// Меняет системный заголовок окна, например, чтобы показывать текущий FPS.
bool setWindowTitle(const char* title, Error& error);

// Отличает нормальное закрытие от ошибки; error читается только при Failed.
EventResult processEvents(Error& error);

// Проверяет, нажата ли клавиша, соответствующая игровому действию.
bool isKeyDown(Key key);

// Показывает нарисованный кадр из заднего буфера окна.
bool present(Error& error);

// Освобождает созданные ресурсы ОС и графической подсистемы.
// Продолжает очистку после отказа отдельной операции; заменяет errors списком ошибок этого вызова.
void shutdown(std::vector<Error>& errors);
}
