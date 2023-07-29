/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/can/bus.h"

namespace vcml {
namespace can {

void bus::can_receive(const can_target_socket& rx, can_frame& frame) {
    const can_initiator_socket& sender = peer_of(rx);
    for (auto& tx : can_tx) {
        if (tx.second != &sender)
            tx.second->send(frame);
    }
}

bus::bus(const sc_module_name& nm):
    module(nm), can_host(), m_next_id(0), can_tx("can_tx"), can_rx("can_rx") {
    // nothing to do
}

void bus::bind(can_initiator_socket& tx, can_target_socket& rx) {
    while (can_tx.exists(m_next_id) || can_rx.exists(m_next_id))
        m_next_id++;

    can_tx[m_next_id].bind(rx);
    tx.bind(can_rx[m_next_id]);
    m_next_id++;
}

VCML_EXPORT_MODEL(vcml::can::bus, name, args) {
    return new bus(name);
}

} // namespace can
} // namespace vcml
