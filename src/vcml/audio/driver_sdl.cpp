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

static void sdl_audio_tx(void* userdata, Uint8* buffer, int len) {
    driver_sdl* driver = (driver_sdl*)userdata;
    driver->handle_tx(buffer, len);
}

static void sdl_audio_rx(void* userdata, Uint8* buffer, int len) {
    driver_sdl* driver = (driver_sdl*)userdata;
    driver->handle_rx(buffer, len);
}

SDL_AudioDeviceID driver_sdl::open(bool capture, u32 format, u32 channels,
                                   u32 rate) {
    m_buffer.clear();

    SDL_AudioDeviceID device = capture ? m_input : m_output;
    if (device) {
        if (format == m_format && channels == m_channels && m_rate == rate)
            return device;

        SDL_CloseAudioDevice(m_output);
        m_format = FORMAT_INVALID;
        m_channels = 0;
        m_rate = 0;
    }

    if (!output_supports_format(format))
        return 0;

    SDL_AudioSpec spec{};
    spec.freq = rate;
    spec.format = sdl_format_from_vcml(format);
    spec.channels = channels;
    spec.samples = 1024;
    spec.callback = capture ? sdl_audio_rx : sdl_audio_tx;
    spec.userdata = this;

    device = SDL_OpenAudioDevice(nullptr, capture, &spec, nullptr, 0);
    if (!device)
        return 0;

    m_format = format;
    m_channels = channels;
    m_rate = rate;

    // we choose a buffer size large enough to fit 250ms of audio samples in
    // order to smooth out SystemC lag spikes; larger buffers induce an
    // audible delay in sound playback
    m_maxsz = buffer_size(250, m_format, m_channels, m_rate);
    m_buffer.reserve(m_maxsz);

    log_debug("successfully configured %s stream",
              capture ? "input" : "output");
    log_debug("  format: %s (%u channels)", format_str(m_format), m_channels);
    log_debug(" samples: %uHz", m_rate);
    log_debug("  buffer: %zu bytes", m_maxsz);
    return device;
}

void driver_sdl::push_buffer(const void* buf, size_t len) {
    size_t rem = m_maxsz - m_buffer.size();
    if (len > rem) {
        len = rem;
        log_debug("audio buffer overflow, samples dropped");
    }

    m_buffer.insert(m_buffer.end(), (const u8*)buf, (const u8*)buf + len);
}

void driver_sdl::pop_buffer(void* buf, size_t len) {
    size_t bytes = min(len, m_buffer.size());
    memcpy(buf, m_buffer.data(), bytes);
    if (bytes < len)
        fill_silence((u8*)buf + bytes, len - bytes, m_format);
    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + bytes);
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
    m_output = open(false, format, channels, rate);
    return m_output != 0;
}

void driver_sdl::output_enable(bool enable) {
    lock_guard<mutex> guard(m_mtx);
    if (m_output)
        SDL_PauseAudioDevice(m_output, enable ? 0 : 1);
}

void driver_sdl::output_xfer(const void* buf, size_t len) {
    lock_guard<mutex> guard(m_mtx);
    if (m_output)
        push_buffer(buf, len);
}

void driver_sdl::output_shutdown() {
    lock_guard<mutex> guard(m_mtx);
    if (m_output) {
        SDL_CloseAudioDevice(m_output);
        m_output = 0;
    }
}

size_t driver_sdl::input_min_channels() {
    return 1;
}

size_t driver_sdl::input_max_channels() {
    return 1;
}

bool driver_sdl::input_supports_format(u32 format) {
    switch (format) {
    case FORMAT_U8:
    case FORMAT_S16LE:
    case FORMAT_S32LE:
    case FORMAT_F32LE:
        return true;

    case FORMAT_S8:
    case FORMAT_U16LE:
    case FORMAT_U16BE:
    case FORMAT_S16BE:
    case FORMAT_U32LE:
    case FORMAT_U32BE:
    case FORMAT_S32BE:
    case FORMAT_F32BE:
    default:
        return false;
    }
}

bool driver_sdl::input_supports_rate(u32 rate) {
    return true;
}

bool driver_sdl::input_configure(u32 format, u32 channels, u32 rate) {
    lock_guard<mutex> guard(m_mtx);
    m_input = open(true, format, channels, rate);
    return m_input != 0;
}

void driver_sdl::input_enable(bool enable) {
    lock_guard<mutex> guard(m_mtx);
    if (m_input)
        SDL_PauseAudioDevice(m_input, enable ? 0 : 1);
}

void driver_sdl::input_xfer(void* buf, size_t len) {
    lock_guard<mutex> guard(m_mtx);
    pop_buffer(buf, len);
}

void driver_sdl::input_shutdown() {
    lock_guard<mutex> guard(m_mtx);
    if (m_input) {
        SDL_CloseAudioDevice(m_input);
        m_input = 0;
    }
}

void driver_sdl::handle_tx(void* buf, size_t len) {
    lock_guard<mutex> guard(m_mtx);
    pop_buffer(buf, len);
}

void driver_sdl::handle_rx(const void* buf, size_t len) {
    lock_guard<mutex> guard(m_mtx);
    if (m_input)
        push_buffer(buf, len);
}

} // namespace audio
} // namespace vcml
