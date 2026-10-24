#pragma once

#include "platform.h"
#include "renderer.h"

#include <optional>
#include <vector>

namespace Engine
{
struct ApplicationOptions
{
    Platform::WindowOptions window;
    float maxDeltaTime = 0.08F;
    // Ноль отключает программное ограничение FPS.
    double targetFramesPerSecond = 120.0;
    // nullopt сохраняет режим драйвера. Явное включение VSync отключает программное ожидание.
    std::optional<bool> verticalSync;
};

// Длительности частей предыдущего завершённого кадра в секундах.
struct FrameTimings
{
    double events = 0;
    double update = 0;
    double preparation = 0;
    double rendering = 0;
    double presentation = 0;
    double waiting = 0;
    double total = 0;
};

struct FrameInfo
{
    Platform::WindowState window;
    float deltaTime = 0;
    // Игровое время накапливает ограниченный dT и не идёт во время паузы.
    double elapsedTime = 0;
    // Реальное время с начала цикла, включая паузы и сворачивание.
    double realTime = 0;
    double framesPerSecond = 0;
    bool fpsUpdated = false;
    bool paused = false;
    FrameTimings timings;
};

// Однопоточный цикл одного окна. Объект приложения рассчитан на один run().
class Application
{
public:
    virtual ~Application() = default;

    // false означает ошибку: основная ошибка идёт первой, ошибки очистки — следом.
    // Очистка приложения выполняется до уничтожения контекста OpenGL, в том числе
    // при отказе onStart(). Ошибки не показываются пользователю внутри движка.
    bool run(const ApplicationOptions& options, std::vector<Platform::Error>& errors);

    // Явная пауза независима от автоматической паузы свёрнутого окна.
    // Изменения из onInput() применяются к обновлению текущего кадра.
    void setPaused(bool paused);
    bool isPaused() const;
    // Применяет VSync к текущему окну и согласует его с программным ограничителем.
    bool setVSync(bool enabled, Platform::Error& error);
    // Завершение после текущего обработчика, без следующих стадий кадра.
    void requestExit();

protected:
    // Рендерер принадлежит приложению. initialize() вызывается из onStart(); после
    // onStop() цикл автоматически вызывает shutdown(), пока контекст ещё существует.
    Renderer2D& getRenderer();
    virtual bool onStart(Platform::Error& error);
    // Вызывается и на паузе, и при сворачивании. deltaTime и elapsedTime здесь
    // относятся к предыдущему обновлению; текущий dT вычисляется после onInput().
    virtual bool onInput(const FrameInfo& frame, Platform::Error& error);
    virtual bool onUpdate(const FrameInfo& frame, Platform::Error& error);
    // Подготовка и рисование вызываются и на явной паузе, но не при сворачивании.
    virtual bool onPrepareFrame(const FrameInfo& frame, Platform::Error& error);
    virtual bool onRender(const FrameInfo& frame, Platform::Error& error) = 0;
    virtual void onStop(std::vector<Platform::Error>& errors);

private:
    Renderer2D graphics;
    bool paused = false;
    bool exitRequested = false;
    bool started = false;
    bool verticalSyncEnabled = false;
};
}
