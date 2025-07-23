/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_AUDIO_SDL_H
#define VCML_AUDIO_SDL_H

#include "vcml/audio/driver.h"

#include <SDL.h>

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

class sdl_driver : public driver
{
private:
    sdl_audio& m_audio;

public:
    sdl_driver(): m_audio(sdl_audio::instance()) {}
    virtual ~sdl_driver() = default;

    virtual bool configure_output(u32 format, u32 channels,
                                  u32 rate) override {
        return true;
    }

    virtual void output(void* buf, size_t len) override {}

    virtual void set_output_volume(float volume) override {}
};

} // namespace audio
} // namespace vcml

#endif
