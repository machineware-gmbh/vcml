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
    module& m_parent;
    sdl_audio& m_audio;

    u32 m_format;
    u32 m_channels;
    u32 m_rate;
    SDL_AudioDeviceID m_device;

    mutex m_mtx;
    vector<u8> m_buffer;

public:
    driver_sdl(module& parent);
    virtual ~driver_sdl();

    virtual bool configure_output(u32 format, u32 channels, u32 rate) override;
    virtual void output(void* buf, size_t len) override;
    virtual void set_output_volume(float volume) override;

    void audio_callback(void* buf, size_t len);
};

} // namespace audio
} // namespace vcml

#endif
