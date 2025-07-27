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

static string driver_kind(const string& type) {
    size_t pos = type.find(':');
    return type.substr(0, pos);
}

static string driver_args(const string& type) {
    size_t pos = type.find(':');
    if (pos != string::npos && pos + 1 < type.size())
        return type.substr(pos + 1);
    return "";
}

driver::driver(stream& owner): m_stream(owner), log(owner.log) {
    // nothing to do
}

driver::~driver() {
    // nothing to do
}

driver* driver::create(stream& owner, const string& type) {
    string kind = driver_kind(type);
    string args = driver_args(type);

    if (kind == "wav") {
        string path = args.empty() ? mkstr("%s.wav", owner.name()) : args;
        return new driver_wav(owner, path);
    }

#ifdef HAVE_SDL2
    if (kind == "sdl") {
        int id = args.empty() ? 0 : from_string<int>(args);
        return new driver_sdl(owner, id);
    }
#endif

    VCML_REPORT("unknown audio driver \"%s\"", kind.c_str());
}

} // namespace audio
} // namespace vcml
