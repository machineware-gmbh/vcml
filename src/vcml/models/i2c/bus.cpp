/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/i2c/bus.h"

namespace vcml {
namespace i2c {

bus::bus(const sc_module_name& nm):
    component(nm), i2c_host(), i2c_in("i2c_in"), i2c_out("i2c_out") {
}

void bus::reset() {
    component::reset();
}

void bus::i2c_transport(i2c_target_socket&, i2c_payload& tx) {
    for (auto& port : i2c_out)
        port.second->transport(tx);
}

unsigned int bus::next_free() const {
    unsigned int idx = 0;
    while (i2c_out.exists(idx))
        VCML_ERROR_ON(++idx == 0, "no free ports");
    return idx;
}

void bus::bind(i2c_initiator_socket& initiator) {
    i2c_in.bind(initiator);
}

unsigned int bus::bind(i2c_target_socket& target) {
    unsigned int port = next_free();
    i2c_out[port].bind(target);
    return port;
}

VCML_EXPORT_MODEL(vcml::i2c::bus, name, args) {
    return new bus(name);
}

} // namespace i2c
} // namespace vcml
