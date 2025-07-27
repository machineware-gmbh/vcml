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

stream::stream(const sc_module_name& nm):
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

stream::~stream() {
    for (driver* drv : m_drivers)
        delete drv;
}

} // namespace audio
} // namespace vcml
