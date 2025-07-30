/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/audio/ostream.h"
#include "vcml/audio/driver.h"
#include "vcml/audio/driver_wav.h"

#ifdef HAVE_SDL2
#include "vcml/audio/driver_sdl.h"
#endif

namespace vcml {
namespace audio {

driver::driver(stream& owner): m_stream(owner), log(owner.log) {
    // nothing to do
}

driver::~driver() {
    // nothing to do
}

driver* driver::create(stream& owner, const string& type) {
    if (starts_with(type, "wav"))
        return new driver_wav(owner, type);

#ifdef HAVE_SDL2
    if (starts_with(type, "sdl"))
        return new driver_sdl(owner, type);
#endif

    VCML_REPORT("unknown audio driver \"%s\"", type.c_str());
}

} // namespace audio
} // namespace vcml
