/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/audio/istream.h"

namespace vcml {
namespace audio {

istream::istream(const sc_module_name& nm): stream(nm) {
    // nothing to do
}

istream::~istream() {
    // nothing to do
}

size_t istream::min_channels() {
    size_t min_channels = 1;
    for (auto* drv : m_drivers)
        min_channels = max(min_channels, drv->input_min_channels());
    return min_channels;
}

size_t istream::max_channels() {
    size_t max_channels = 8;
    for (auto* drv : m_drivers)
        max_channels = min(max_channels, drv->input_max_channels());
    return max_channels;
}

bool istream::supports_format(u32 format) {
    if (format == FORMAT_INVALID)
        return false;

    for (auto* drv : m_drivers) {
        if (!drv->input_supports_format(format))
            return false;
    }

    return true;
}

bool istream::supports_rate(u32 rate) {
    for (auto* drv : m_drivers) {
        if (!drv->input_supports_rate(rate))
            return false;
    }

    return true;
}

bool istream::configure(u32 format, u32 channels, u32 rate) {
    bool ok = true;
    for (driver* drv : m_drivers)
        ok &= drv->input_configure(format, channels, rate);
    return ok;
}

void istream::start() {
    for (driver* drv : m_drivers)
        drv->input_enable(true);
}

void istream::stop() {
    for (driver* drv : m_drivers)
        drv->input_enable(false);
}

void istream::shutdown() {
    for (driver* drv : m_drivers)
        drv->input_shutdown();
}

void istream::xfer(void* buf, size_t len) {
    for (driver* drv : m_drivers)
        drv->input_xfer(buf, len);
}

} // namespace audio
} // namespace vcml
