#include <miniaudio.h>

#include <array>
#include <iostream>
#include <string>

namespace
{
constexpr ma_uint32 SampleRate = 48000;
constexpr ma_uint32 ChannelCount = 2;
constexpr ma_uint64 FrameCount = 4;
constexpr std::array<ma_int16, FrameCount * ChannelCount> ExpectedSamples = {
    0, 32767, -32768, 0, 12345, -12345, -1, 1
};

ma_decoder_config decoderConfig()
{
    ma_decoder_config config = ma_decoder_config_init(ma_format_s16, 0, 0);
    config.encodingFormat = ma_encoding_format_wav;
    return config;
}

bool checkSamples(const char* path)
{
    const ma_decoder_config config = decoderConfig();
    ma_decoder decoder;
    const ma_result initialized = ma_decoder_init_file(path, &config, &decoder);
    if (initialized != MA_SUCCESS) {
        std::cerr << "Cannot decode WAV: " << path << " (error " << initialized << ")\n";
        return false;
    }

    ma_format format = ma_format_unknown;
    ma_uint32 channels = 0;
    ma_uint32 sampleRate = 0;
    ma_uint64 length = 0;
    bool passed = ma_decoder_get_data_format(&decoder, &format, &channels, &sampleRate, nullptr, 0) == MA_SUCCESS
        && format == ma_format_s16 && channels == ChannelCount && sampleRate == SampleRate
        && ma_decoder_get_length_in_pcm_frames(&decoder, &length) == MA_SUCCESS && length == FrameCount;
    if (!passed) {
        std::cerr << "Expected 48000 Hz stereo s16 WAV with 4 PCM frames\n";
    } else {
        std::array<ma_int16, ExpectedSamples.size()> samples = {};
        ma_uint64 framesRead = 0;
        const ma_result result = ma_decoder_read_pcm_frames(&decoder, samples.data(), FrameCount, &framesRead);
        passed = result == MA_SUCCESS && framesRead == FrameCount && samples == ExpectedSamples;
        if (!passed) {
            std::cerr << "Decoded WAV frame count or sample values differ from expected data\n";
        } else {
            std::array<ma_int16, ChannelCount> extraFrame = {};
            passed = ma_decoder_read_pcm_frames(&decoder, extraFrame.data(), 1, &framesRead) == MA_AT_END
                && framesRead == 0;
            if (!passed) {
                std::cerr << "Expected end of WAV after 4 PCM frames\n";
            }
        }
    }

    const ma_result released = ma_decoder_uninit(&decoder);
    if (released != MA_SUCCESS) {
        std::cerr << "Cannot release WAV decoder (error " << released << ")\n";
    }
    return passed && released == MA_SUCCESS;
}

}

int main(int argumentCount, char* arguments[])
{
    if (argumentCount != 3) {
        std::cerr << "Usage: miniaudio_test <samples> <file>\n";
        return 1;
    }

    const std::string scenario = arguments[1];
    bool passed = false;
    if (scenario == "samples") {
        passed = checkSamples(arguments[2]);
    } else {
        std::cerr << "Unknown scenario: " << arguments[1] << "\n";
    }

    return passed ? 0 : 1;
}
