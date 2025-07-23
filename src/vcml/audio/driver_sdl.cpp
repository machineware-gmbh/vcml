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

driver_sdl::driver_sdl(): m_audio(sdl_audio::instance()) {
}

driver_sdl::~driver_sdl() {
    // TODO
}

bool driver_sdl::configure_output(u32 format, u32 channels, u32 rate) {
    return true;
}

void driver_sdl::output(void* buf, size_t len) {
    // TODO
}

void driver_sdl::set_output_volume(float volume) {
    // TODO
}

} // namespace audio
} // namespace vcml
