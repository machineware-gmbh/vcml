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
    sdl_audio() { SDL_InitSubSystem(SDL_INIT_AUDIO); }
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
    case FORMAT_U32LE:
    case FORMAT_U32BE:
    default:
        VCML_ERROR("unsupported format: 0x%x", format);
    }
}

driver_sdl::driver_sdl(module& parent):
    m_parent(parent),
    m_audio(sdl_audio::instance()),
    m_format(),
    m_channels(),
    m_rate(),
    m_device(),
    m_mtx(),
    m_buffer() {
}

driver_sdl::~driver_sdl() {
    lock_guard<mutex> guard(m_mtx);

    if (m_device) {
        SDL_CloseAudioDevice(m_device);
        m_device = 0;
    }
}

static void sdl_audio_callback(void* userdata, Uint8* buffer, int len) {
    driver_sdl* driver = (driver_sdl*)userdata;
    driver->audio_callback(buffer, len);
}

bool driver_sdl::configure_output(u32 format, u32 channels, u32 rate) {
    lock_guard<mutex> guard(m_mtx);

    SDL_AudioSpec spec{};
    spec.freq = rate;
    spec.format = sdl_format_from_vcml(format);
    spec.channels = channels;
    spec.samples = 1024;
    spec.callback = sdl_audio_callback;
    spec.userdata = this;

    m_device = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
    if (!m_device)
        return false;

    m_format = format;
    m_channels = channels;
    m_rate = rate;
    m_parent.log_debug(
        "successfully configured output stream: %s, %u channels, %uHz",
        format_str(format), channels, rate);
    SDL_PauseAudioDevice(m_device, 0);
    return true;
}

void driver_sdl::output(void* buf, size_t len) {
    lock_guard<mutex> guard(m_mtx);
    log_debug("received %zu bytes audio data", len);
    if (m_buffer.size() > 4 * KiB) {
        log_warn("audio buffer overflow");
        return;
    }

    m_buffer.insert(m_buffer.end(), (u8*)buf, (u8*)buf + len);
}

void driver_sdl::set_output_volume(float volume) {
    // TODO
}

void driver_sdl::audio_callback(void* buf, size_t len) {
    lock_guard<mutex> guard(m_mtx);
    size_t bytes = min(len, m_buffer.size());
    memcpy(buf, m_buffer.data(), bytes);
    if (bytes < len)
        fill_silence((u8*)buf + bytes, len - bytes, m_format);
    m_parent.log_debug("fetching %zu bytes audio data, %zu bytes silence",
                       bytes, len - bytes);
    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + bytes);
}

} // namespace audio
} // namespace vcml
