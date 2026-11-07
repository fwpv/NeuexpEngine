#pragma once

#include "sprite.h"
#include "resources.h"

#include <cstddef>
#include <vector>

namespace Engine
{
struct AnimationFrame
{
    Rect textureRect;
    double duration = 0.1; // Секунды, конечное положительное число.
};

enum class AnimationMode
{
    Once,
    Loop,
    // Удерживает крайний кадр, продолжая отсчёт его интервалов; finished не взводится.
    Clamp
};

enum class AnimationDirection
{
    Forward,
    Backward
};

// Спрайт с последовательностью одинаковых по размеру участков одной текстуры.
// Используется по значению, как Sprite; удаление через указатель Sprite не поддерживается.
class Animation : public Sprite
{
public:
    // При отказе объект сохраняется. При успехе — начальный по направлению кадр, остановка,
    // скорость 1, сброс finished и начальный размер кадра; оформление, режим и направление сохраняются.
    bool load(const Texture& source, const std::vector<AnimationFrame>& frames, ResourceError& error);
    bool load(const Texture& source, const std::vector<Rect>& frames, double duration, ResourceError& error);

    // play продолжает паузу; после завершения требуется restart или stop + play.
    void play();
    void pause();
    // stop возвращает начальный по направлению кадр и сбрасывает finished; restart дополнительно запускает.
    void stop();
    void restart();
    // Меняет режим без сброса времени и флага завершения.
    void setMode(AnimationMode value);
    // Меняет порядок обхода, сохраняя текущий кадр и время внутри него.
    // После завершения снимает finished; запуск по-прежнему выполняет play().
    void setDirection(AnimationDirection value);
    // Индекс с нуля. Сбрасывает время внутри кадра и finished, сохраняя playing.
    // Некорректный индекс вызывает std::out_of_range без изменения объекта.
    void setFrame(std::size_t index);
    std::size_t frameCount() const { return frames.size(); }
    AnimationDirection direction() const { return playbackDirection; }
    // Конечность скорости и deltaTime — предусловие вызывающего кода.
    // Отрицательные значения отклоняются с std::invalid_argument.
    // Переполнение deltaTime * speed также отклоняется без изменения состояния.
    void setSpeed(double value);
    // Вызывать на этапе обновления. Меняет только textureRect и состояние проигрывания.
    void update(double deltaTime);

    bool isPlaying() const { return playing; }
    // Устойчивый флаг завершения Once; снимается load, stop, restart, setFrame
    // или сменой направления.
    bool isFinished() const { return finished; }
    std::size_t currentFrame() const { return frameIndex; }
    // Время от начала прохода в выбранном направлении; при завершении — полная длительность.
    double elapsedTime() const { return elapsed; }
    double speed() const { return playbackSpeed; }
    AnimationMode mode() const { return playbackMode; }

private:
    void selectFrame();
    double frameStart(std::size_t index) const;

    std::vector<AnimationFrame> frames;
    double duration = 0;
    double elapsed = 0;
    double playbackSpeed = 1;
    std::size_t frameIndex = 0;
    AnimationMode playbackMode = AnimationMode::Loop;
    AnimationDirection playbackDirection = AnimationDirection::Forward;
    bool playing = false;
    bool finished = false;
};
}
