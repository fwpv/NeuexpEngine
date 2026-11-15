#pragma once

#include "resources.h"
#include "sound.h"

#include <memory>

namespace Nehopio
{
// Воспроизведение эффектов через miniaudio. Движок аудио ещё не содержит,
// поэтому демо использует библиотеку напрямую, как и тест звука в tests.
// При отсутствии звукового устройства демо продолжает работать без звука.
class Audio
{
public:
    Audio();
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    bool load(const NeuexpEngine::ResourceFiles& files, NeuexpEngine::ResourceError& error);
    void play(Sound sound);
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
