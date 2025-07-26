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

namespace vcml {
namespace audio {

ostream::ostream(const sc_module_name& nm): stream(nm) {
    // nothing to do
}

ostream::~ostream() {
    // nothing to do
}

size_t ostream::min_channels() {
    size_t min_channels = 1;
    for (auto* drv : m_drivers)
        min_channels = max(min_channels, drv->output_min_channels());
    return min_channels;
}

size_t ostream::max_channels() {
    size_t max_channels = 8;
    for (auto* drv : m_drivers)
        max_channels = min(max_channels, drv->output_max_channels());
    return max_channels;
}

bool ostream::supports_format(u32 format) {
    if (format == FORMAT_INVALID)
        return false;

    for (auto* drv : m_drivers) {
        if (!drv->output_supports_format(format))
            return false;
    }

    return true;
}

bool ostream::supports_rate(u32 rate) {
    for (auto* drv : m_drivers) {
        if (!drv->output_supports_rate(rate))
            return false;
    }

    return true;
}

bool ostream::configure(u32 format, u32 channels, u32 rate) {
    bool ok = true;
    for (driver* drv : m_drivers) {
        ok &= drv->output_configure(format, channels, rate);
        if (!ok)
            log_warn("config failed");
    }
    return ok;
}

void ostream::start() {
    for (driver* drv : m_drivers)
        drv->output_enable(true);
}

void ostream::stop() {
    for (driver* drv : m_drivers)
        drv->output_enable(false);
}

void ostream::shutdown() {
    for (driver* drv : m_drivers)
        drv->output_shutdown();
}

void ostream::xfer(const void* buf, size_t len) {
    for (driver* drv : m_drivers)
        drv->output_xfer(buf, len);
}

} // namespace audio
} // namespace vcml
