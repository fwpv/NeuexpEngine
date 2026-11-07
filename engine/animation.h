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
    Loop
};

// Спрайт с последовательностью одинаковых по размеру участков одной текстуры.
// Используется по значению, как Sprite; удаление через указатель Sprite не поддерживается.
class Animation : public Sprite
{
public:
    // При отказе объект сохраняется. При успехе — первый кадр, остановка,
    // скорость 1, сброс finished и начальный размер кадра; оформление и режим сохраняются.
    bool load(const Texture& source, const std::vector<AnimationFrame>& frames, ResourceError& error);
    bool load(const Texture& source, const std::vector<Rect>& frames, double duration, ResourceError& error);

    // play продолжает паузу; после завершения требуется restart или stop + play.
    void play();
    void pause();
    // stop возвращает первый кадр и сбрасывает finished; restart дополнительно запускает.
    void stop();
    void restart();
    // Меняет режим без сброса времени и флага завершения.
    void setMode(AnimationMode value);
    // Скорость >= 0, deltaTime >= 0, оба конечны; иначе std::invalid_argument.
    // Переполнение deltaTime * speed также отклоняется без изменения состояния.
    void setSpeed(double value);
    // Вызывать на этапе обновления. Меняет только textureRect и состояние проигрывания.
    void update(double deltaTime);

    bool isPlaying() const { return playing; }
    // Устойчивый флаг завершения Once; снимается load, stop или restart.
    bool isFinished() const { return finished; }
    std::size_t currentFrame() const { return frameIndex; }
    // Время внутри текущего прохода; при завершении равно полной длительности.
    double elapsedTime() const { return elapsed; }
    double speed() const { return playbackSpeed; }
    AnimationMode mode() const { return playbackMode; }

private:
    void selectFrame();

    std::vector<AnimationFrame> frames;
    double duration = 0;
    double elapsed = 0;
    double playbackSpeed = 1;
    std::size_t frameIndex = 0;
    AnimationMode playbackMode = AnimationMode::Loop;
    bool playing = false;
    bool finished = false;
};
}
