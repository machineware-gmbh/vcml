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

ostream::ostream(const sc_module_name& nm):
    module(nm), m_drivers(), drivers("drivers") {
    vector<string> types = split(drivers);
    for (const auto& type : types) {
        try {
            driver* drv = driver::create(*this, type);
            m_drivers.push_back(drv);
        } catch (std::exception& ex) {
            log.warn(ex);
        }
    }
}

ostream::~ostream() {
    for (driver* drv : m_drivers)
        delete drv;
}

bool ostream::configure(u32 format, u32 channels, u32 rate) {
    bool ok = true;
    for (driver* drv : m_drivers)
        ok &= drv->configure_output(format, channels, rate);
    return ok;
}

void ostream::output(void* buf, size_t len) {
    for (driver* drv : m_drivers)
        drv->output(buf, len);
}

void ostream::set_volume(float volume) {
    if (volume < 0.0)
        volume = 0.0;
    if (volume > 1.0)
        volume = 1.0;

    for (driver* drv : m_drivers)
        drv->set_output_volume(volume);
}

} // namespace audio
} // namespace vcml
