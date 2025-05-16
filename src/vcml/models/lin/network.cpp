/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/lin/network.h"

namespace vcml {
namespace lin {

void network::lin_receive(const lin_target_socket& rx, lin_payload& tx) {
    for (auto [id, port] : lin_out) {
        port->transport(tx);
        if (tx.status != LIN_INCOMPLETE)
            break;
    }
}

network::network(const sc_module_name& nm):
    module(nm),
    lin_host(),
    m_next_id(0),
    lin_out("lin_out"),
    lin_in("lin_in") {
    // nothing to do
}

void network::bind(lin_initiator_socket& socket) {
    socket.bind(lin_in);
}

void network::bind(lin_target_socket& socket) {
    while (lin_out.exists(m_next_id))
        m_next_id++;

    lin_out[m_next_id].bind(socket);
    m_next_id++;
}

VCML_EXPORT_MODEL(vcml::lin::network, name, args) {
    return new network(name);
}

} // namespace lin
} // namespace vcml
