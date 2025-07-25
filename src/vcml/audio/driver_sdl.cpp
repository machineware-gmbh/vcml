/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/audio/driver_sdl.h"

namespace vcml {
namespace audio {

class sdl_audio
{
private:
    sdl_audio() {
        SDL_InitSubSystem(SDL_INIT_AUDIO);
        log_debug("SDL: using audiodriver \"%s\"",
                  SDL_GetCurrentAudioDriver());
    }

    ~sdl_audio() { SDL_QuitSubSystem(SDL_INIT_AUDIO); }

public:
    static sdl_audio& instance() {
        static sdl_audio singleton;
        return singleton;
    }
};

SDL_AudioFormat sdl_format_from_vcml(u32 format) {
    switch (format) {
    case FORMAT_U8:
        return AUDIO_U8;
    case FORMAT_S8:
        return AUDIO_S8;
    case FORMAT_U16LE:
        return AUDIO_U16LSB;
    case FORMAT_S16LE:
        return AUDIO_S16LSB;
    case FORMAT_U16BE:
        return AUDIO_U16MSB;
    case FORMAT_S16BE:
        return AUDIO_S16MSB;
    case FORMAT_S32LE:
        return AUDIO_S32LSB;
    case FORMAT_S32BE:
        return AUDIO_S32MSB;
    case FORMAT_F32LE:
        return AUDIO_F32LSB;
    case FORMAT_F32BE:
        return AUDIO_F32MSB;
    case FORMAT_U32LE:
    case FORMAT_U32BE:
    default:
        VCML_ERROR("unsupported format: 0x%x", format);
    }
}

driver_sdl::driver_sdl(stream& owner, int id):
    driver(owner),
    m_audio(sdl_audio::instance()),
    m_format(FORMAT_INVALID),
    m_channels(),
    m_rate(),
    m_maxsz(),
    m_output(),
    m_mtx(),
    m_buffer() {
}

driver_sdl::~driver_sdl() {
    lock_guard<mutex> guard(m_mtx);

    if (m_output) {
        SDL_CloseAudioDevice(m_output);
        m_output = 0;
    }
}

static void sdl_audio_callback(void* userdata, Uint8* buffer, int len) {
    driver_sdl* driver = (driver_sdl*)userdata;
    driver->audio_callback(buffer, len);
}

size_t driver_sdl::output_min_channels() {
    return 1;
}

size_t driver_sdl::output_max_channels() {
    return 8;
}

bool driver_sdl::output_supports_format(u32 format) {
    switch (format) {
    case FORMAT_U8:
    case FORMAT_S8:
    case FORMAT_U16LE:
    case FORMAT_U16BE:
    case FORMAT_S16LE:
    case FORMAT_S16BE:
    case FORMAT_S32LE:
    case FORMAT_S32BE:
    case FORMAT_F32LE:
    case FORMAT_F32BE:
        return true;

    case FORMAT_U32LE:
    case FORMAT_U32BE:
    default:
        return false;
    }
}

bool driver_sdl::output_supports_rate(u32 rate) {
    return (rate >= 8000) && (rate <= 192000);
}

bool driver_sdl::output_configure(u32 format, u32 channels, u32 rate) {
    lock_guard<mutex> guard(m_mtx);

    m_buffer.clear();

    if (m_output) {
        if (format == m_format && channels == m_channels && m_rate == rate)
            return true;

        SDL_CloseAudioDevice(m_output);
        m_format = FORMAT_INVALID;
        m_channels = 0;
        m_rate = 0;
    }

    if (!output_supports_format(format))
        return false;

    SDL_AudioSpec spec{};
    spec.freq = rate;
    spec.format = sdl_format_from_vcml(format);
    spec.channels = channels;
    spec.samples = 1024;
    spec.callback = sdl_audio_callback;
    spec.userdata = this;

    m_output = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
    if (!m_output)
        return false;

    m_format = format;
    m_channels = channels;
    m_rate = rate;

    // we choose a buffer size large enough to fit 250ms of audio samples in
    // order to smooth out SystemC lag spikes; larger buffers induce an
    // audible delay in sound playback
    m_maxsz = buffer_size(250, m_format, m_channels, m_rate);
    m_buffer.reserve(m_maxsz);

    log_debug("successfully configured output stream");
    log_debug("  format: %s (%u channels)", format_str(m_format), m_channels);
    log_debug(" samples: %uHz", m_rate);
    log_debug("  buffer: %zu bytes", m_maxsz);

    return true;
}

void driver_sdl::output_enable(bool enable) {
    if (m_output)
        SDL_PauseAudioDevice(m_output, enable ? 0 : 1);
}

void driver_sdl::output(void* buf, size_t len) {
    lock_guard<mutex> guard(m_mtx);
    if (!m_output)
        return;

    size_t rem = m_maxsz - m_buffer.size();
    if (len > rem) {
        len = rem;
        log_debug("audio buffer overflow, samples dropped");
    }

    m_buffer.insert(m_buffer.end(), (u8*)buf, (u8*)buf + len);
}

void driver_sdl::audio_callback(void* buf, size_t len) {
    lock_guard<mutex> guard(m_mtx);
    size_t bytes = min(len, m_buffer.size());
    memcpy(buf, m_buffer.data(), bytes);
    if (bytes < len)
        fill_silence((u8*)buf + bytes, len - bytes, m_format);
    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + bytes);
}

} // namespace audio
} // namespace vcml
