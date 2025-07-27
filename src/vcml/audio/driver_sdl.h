/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_AUDIO_DRIVER_SDL_H
#define VCML_AUDIO_DRIVER_SDL_H

#include "vcml/core/types.h"
#include "vcml/core/module.h"
#include "vcml/audio/driver.h"

#include <SDL.h>

namespace vcml {
namespace audio {

class sdl_audio;
class driver_sdl : public driver
{
private:
    sdl_audio& m_audio;

    u32 m_format;
    u32 m_channels;
    u32 m_rate;
    size_t m_maxsz;

    SDL_AudioDeviceID m_output;
    SDL_AudioDeviceID m_input;

    mutex m_mtx;
    vector<u8> m_buffer;

    SDL_AudioDeviceID open(bool capture, u32 format, u32 channels, u32 rate);

    void push_buffer(const void* buf, size_t len);
    void pop_buffer(void* buf, size_t len);

public:
    driver_sdl(stream& owner, int id);
    virtual ~driver_sdl();

    virtual size_t output_min_channels() override;
    virtual size_t output_max_channels() override;
    virtual bool output_supports_format(u32 format) override;
    virtual bool output_supports_rate(u32 rate) override;
    virtual bool output_configure(u32 format, u32 channels, u32 rate) override;
    virtual void output_enable(bool enable) override;
    virtual void output_xfer(const void* buf, size_t len) override;
    virtual void output_shutdown() override;

    virtual size_t input_min_channels() override;
    virtual size_t input_max_channels() override;
    virtual bool input_supports_format(u32 format) override;
    virtual bool input_supports_rate(u32 rate) override;
    virtual bool input_configure(u32 format, u32 channels, u32 rate) override;
    virtual void input_enable(bool enable) override;
    virtual void input_xfer(void* buf, size_t len) override;
    virtual void input_shutdown() override;

    void handle_tx(void* buf, size_t len);
    void handle_rx(const void* buf, size_t len);
};

} // namespace audio
} // namespace vcml

#endif
