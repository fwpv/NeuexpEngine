#pragma once

#include "platform.h"
#include "graphics/renderer.h"
#include "core/math2d.h"

#include <optional>

namespace NeuexpEngine
{
struct ApplicationOptions
{
    Platform::WindowOptions window;
    float maxDeltaTime = 0.08F;
    // Ноль отключает программное ограничение FPS.
    double targetFramesPerSecond = 120.0;
    // nullopt сохраняет режим драйвера. Явное включение VSync отключает программное ожидание.
    std::optional<bool> verticalSync;
    // Ноль оставляет переменный dT кадра из этапа 03. Положительное значение включает
    // постоянный шаг симуляции: onUpdate вызывается столько раз, сколько шагов накопилось,
    // с этим dT, а мир получает воспроизводимую последовательность одинаковых шагов.
    float fixedStepTime = 0;
    // Предел вызовов onUpdate за кадр при постоянном шаге: время сверх него отбрасывается,
    // поэтому после задержки симуляция догоняет не больше этого числа шагов.
    int maxCatchUpSteps = 5;
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
    // Число вызовов onUpdate за кадр: 1 при переменном dT, до maxCatchUpSteps при
    // постоянном шаге и 0 на паузе и при свёрнутом окне.
    int updates = 0;
    // Игровое время накапливает выполненные шаги и не идёт во время паузы.
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

    // false означает ошибку; возвращается первая причина отказа.
    // Дополнительные ошибки очистки выводятся в stderr без диалогов.
    // Очистка приложения выполняется до уничтожения контекста OpenGL, в том числе
    // при отказе onStart(). error очищается перед запуском.
    bool run(const ApplicationOptions& options, Platform::Error& error);

    // Явная пауза независима от автоматической паузы свёрнутого окна.
    // Изменения из onInput() применяются к обновлению текущего кадра.
    void setPaused(bool paused);
    bool isPaused() const;
    // Применяет VSync к текущему окну и согласует его с программным ограничителем.
    bool setVSync(bool enabled, Platform::Error& error);
    // Завершение после текущего обработчика, без следующих стадий кадра.
    void requestExit();

private:
    bool runFrames(const ApplicationOptions& options, Platform::Error& error);
    bool rendererResult(const char* operation, bool success, RenderError error, Platform::Error& result) const;

    Renderer2D graphics;
    bool paused = false;
    bool exitRequested = false;
    bool started = false;
    bool verticalSyncEnabled = false;

protected:
    // Рендерер принадлежит приложению. initialize() вызывается из onStart(); после
    // onStop() цикл автоматически вызывает shutdown(), пока контекст ещё существует.
    Renderer2D& getRenderer();
    // Инициализация и завершение кадра переводят RenderError в ошибку приложения.
    bool initializeRenderer(Platform::Error& error);
    // Пара beginFrame/endFrame управляет кадром getRenderer(); показ выполняет цикл.
    // Ошибки начала и рисования возвращаются при завершении кадра.
    void beginFrame(int width, int height, Color clear);
    bool endFrame(Platform::Error& error);
    virtual bool onStart(Platform::Error& error);
    // Вызывается и на паузе, и при сворачивании. deltaTime и elapsedTime здесь
    // относятся к предыдущему обновлению; текущий dT вычисляется после onInput().
    virtual bool onInput(const FrameInfo& frame, Platform::Error& error);
    virtual bool onUpdate(const FrameInfo& frame, Platform::Error& error);
    // Подготовка отображения: расчёт видов, обновление заголовка и прочие действия
    // перед рисованием. Вызывается и на явной паузе, но не при сворачивании.
    virtual bool onPrepareFrame(const FrameInfo& frame, Platform::Error& error);
    // Формирование и завершение кадра рендерера. После успешного возврата цикл
    // показывает кадр. Вызывается после onPrepareFrame, в том числе на явной паузе.
    virtual bool onRender(const FrameInfo& frame, Platform::Error& error) = 0;
    virtual bool onStop(Platform::Error& error);
};
}
