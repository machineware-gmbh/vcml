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

#include "vcml/audio/driver.h"

#include <SDL.h>

namespace vcml {
namespace audio {

class sdl_audio;
class driver_sdl : public driver
{
private:
    sdl_audio& m_audio;

public:
    driver_sdl();
    virtual ~driver_sdl();

    virtual bool configure_output(u32 format, u32 channels, u32 rate) override;
    virtual void output(void* buf, size_t len) override;
    virtual void set_output_volume(float volume) override;
};

} // namespace audio
} // namespace vcml

#endif
