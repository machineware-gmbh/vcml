/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/audio/driver_wav.h"

namespace vcml {
namespace audio {

driver_wav::driver_wav(const string& path): m_path(path), m_file() {
    // TODO
}

driver_wav::~driver_wav() {
    // TODO
}

bool driver_wav::configure_output(u32 format, u32 channels, u32 rate) {
    m_file.open(m_path);
    if (!m_file)
        return false;

    return true;
}

void driver_wav::output(void* buf, size_t len) {
}

void driver_wav::set_output_volume(float volume) {
    // nothing to do
}

} // namespace audio
} // namespace vcml
