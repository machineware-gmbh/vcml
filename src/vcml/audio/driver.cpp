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

static string wav_file_name(const string& type, const string& defval) {
    size_t pos = type.find(':');
    if (pos != string::npos && pos + 1 < type.size())
        return type.substr(pos + 1);
    else
        return mkstr("%s.wav", defval.c_str());
}

driver* driver::create(ostream& owner, const string& type) {
    string kind = type.substr(0, type.find(':'));
    if (kind == "wav")
        return new driver_wav(wav_file_name(type, owner.name()));

#ifdef HAVE_SDL2
    if (kind == "sdl")
        return new driver_sdl(owner);
#endif

    VCML_REPORT("unknown audio driver \"%s\"", kind.c_str());
}

} // namespace audio
} // namespace vcml
