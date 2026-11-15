#include "audio.h"

#include <miniaudio.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <string>

namespace Nehopio
{
namespace
{
constexpr std::array<const char*, 8> SoundFiles{
    "nehopio/sounds/jump.wav",
    "nehopio/sounds/coin.wav",
    "nehopio/sounds/stomp.wav",
    "nehopio/sounds/break.wav",
    "nehopio/sounds/bump.wav",
    "nehopio/sounds/powerup.wav",
    "nehopio/sounds/win.wav",
    "nehopio/sounds/death.wav",
};
constexpr float EffectVolume = 0.45F;
}

struct Audio::Impl
{
    ma_engine engine{};
    std::array<ma_sound, SoundFiles.size()> sounds{};
    std::size_t initialized = 0;
    bool engineReady = false;
};

Audio::Audio()
    : impl(std::make_unique<Impl>())
{
}

Audio::~Audio()
{
    shutdown();
}

bool Audio::load(const Engine::ResourceFiles& files, Engine::ResourceError& error)
{
    shutdown();
    if (ma_engine_init(nullptr, &impl->engine) != MA_SUCCESS) {
        // Отсутствие устройства не должно мешать запуску демо.
        std::fprintf(stderr, "Nehopio: audio device is unavailable, continuing without sound.\n");
        return true;
    }
    impl->engineReady = true;
    for (std::size_t index = 0; index < SoundFiles.size(); ++index) {
        std::filesystem::path path;
        if (!files.resolve(SoundFiles[index], path, error)) {
            shutdown();
            return false;
        }
        const std::string name = path.u8string();
        if (ma_sound_init_from_file(&impl->engine, name.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr,
                &impl->sounds[index]) != MA_SUCCESS) {
            error = {Engine::ResourceErrorCode::Io, std::string("Cannot load sound: ") + SoundFiles[index]};
            shutdown();
            return false;
        }
        ma_sound_set_volume(&impl->sounds[index], EffectVolume);
        ++impl->initialized;
    }
    return true;
}

void Audio::play(Sound sound)
{
    if (!impl->engineReady) {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(sound);
    if (index >= impl->initialized) {
        return;
    }
    ma_sound_seek_to_pcm_frame(&impl->sounds[index], 0);
    ma_sound_start(&impl->sounds[index]);
}

void Audio::shutdown()
{
    if (!impl) {
        return;
    }
    if (impl->engineReady) {
        for (std::size_t index = 0; index < impl->initialized; ++index) {
            ma_sound_uninit(&impl->sounds[index]);
        }
        impl->initialized = 0;
        ma_engine_uninit(&impl->engine);
        impl->engineReady = false;
    }
}
}
