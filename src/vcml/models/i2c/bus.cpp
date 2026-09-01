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

unsigned int bus::bind(i2c_initiator_socket& initiator) {
    unsigned int port = i2c_in.next_index();
    i2c_in[port].bind(initiator);
    return port;
}

unsigned int bus::bind(i2c_target_socket& target) {
    unsigned int port = i2c_out.next_index();
    i2c_out[port].bind(target);
    return port;
}

VCML_EXPORT_MODEL(vcml::i2c::bus, name, args) {
    return new bus(name);
}

} // namespace i2c

static void i2c_bind_socket(i2c::bus& bus, sc_object& socket) {
    if (auto* initiator = dynamic_cast<i2c_initiator_socket*>(&socket)) {
        bus.bind(*initiator);
        return;
    }

    if (auto* target = dynamic_cast<i2c_target_socket*>(&socket)) {
        bus.bind(*target);
        return;
    }

    VCML_ERROR("%s is not a valid i2c socket", socket.name());
}

void i2c_bind(i2c::bus& bus, sc_object& socket, string port) {
    sc_object* child = find_child(socket, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", socket.name(), port.c_str());
    i2c_bind_socket(bus, *child);
}

void i2c_bind(i2c::bus& bus, sc_object& socket, string portarray, size_t idx) {
    sc_object* child = find_child(socket, portarray);
    VCML_ERROR_ON(!child, "%s.%s does not exist", socket.name(),
                  portarray.c_str());

    auto* array = dynamic_cast<socket_array_if*>(child);
    VCML_ERROR_ON(!array, "%s is not a valid i2c socket array", child->name());

    i2c_bind_socket(bus, *array->fetch(idx, true));
}

void i2c_bind(i2c::bus& bus, const string& socket) {
    i2c_bind_socket(bus, find_socket(socket));
}

void i2c_bind(i2c::bus& bus, const string& socket, size_t idx) {
    i2c_bind_socket(bus, find_socket(socket, idx));
}

} // namespace vcml
