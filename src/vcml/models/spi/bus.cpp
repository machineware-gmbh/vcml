/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/spi/bus.h"

namespace vcml {
namespace spi {

bus::bus(const sc_module_name& nm):
    component(nm), spi_host(), spi_in("spi_in"), spi_out("spi_out"), cs("cs") {
}

bus::~bus() {
    // nothing to do
}

void bus::reset() {
    component::reset();
}

bool bus::is_valid(unsigned int port) const {
    if (!cs.exists(port) || !spi_out.exists(port))
        return false;
    if (!stl_contains(m_csmode, port))
        return false;
    return true;
}

bool bus::is_active(unsigned int port) const {
    if (!is_valid(port))
        return false;
    return cs[port].read() == m_csmode.at(port);
}

bool bus::is_active_high(unsigned int port) const {
    if (!is_valid(port))
        return false;
    return m_csmode.at(port);
}

bool bus::is_active_low(unsigned int port) const {
    if (!is_valid(port))
        return false;
    return !m_csmode.at(port);
}

void bus::spi_transport(const spi_target_socket&, spi_payload& spi) {
    for (auto port : cs) {
        if (is_active(port.first))
            spi_out[port.first].transport(spi);
    }
}

unsigned int bus::next_free() const {
    unsigned int idx = 0;
    while (spi_out.exists(idx) || cs.exists(idx))
        VCML_ERROR_ON(++idx == 0, "no free ports");
    return idx;
}

void bus::bind(spi_initiator_socket& initiator) {
    spi_in.bind(initiator);
}

unsigned int bus::bind(spi_target_socket& target, gpio_initiator_socket& s,
                       bool cs_active_high) {
    unsigned int port = next_free();
    spi_out[port].bind(target);
    s.bind(cs[port]);
    m_csmode[port] = cs_active_high;
    return port;
}

VCML_EXPORT_MODEL(vcml::spi::bus, name, args) {
    return new bus(name);
}

} // namespace spi
} // namespace vcml
