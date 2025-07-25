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

driver_wav::driver_wav(stream& owner, const string& path):
    driver(owner), m_path(path), m_file(), m_enabled() {
    // TODO
}

driver_wav::~driver_wav() {
    // TODO
}

size_t driver_wav::output_min_channels() {
    return 1;
}

size_t driver_wav::output_max_channels() {
    return 2;
}

bool driver_wav::output_supports_format(u32 format) {
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

bool driver_wav::output_supports_rate(u32 rate) {
    return true;
}

bool driver_wav::output_configure(u32 format, u32 channels, u32 rate) {
    m_file.open(m_path);
    if (!m_file)
        return false;

    return true;
}

void driver_wav::output_enable(bool enable) {
    m_enabled = enable;
}

void driver_wav::output(void* buf, size_t len) {
    // TODO
}

} // namespace audio
} // namespace vcml
