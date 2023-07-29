/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/ethernet/network.h"

namespace vcml {
namespace ethernet {

void network::eth_receive(const eth_target_socket& rx, eth_frame& frame) {
    const eth_initiator_socket& sender = peer_of(rx);
    for (auto& tx : eth_tx) {
        if (tx.second != &sender)
            tx.second->send(frame);
    }
}

network::network(const sc_module_name& nm):
    module(nm), eth_host(), m_next_id(0), eth_tx("eth_tx"), eth_rx("eth_rx") {
    // nothing to do
}

void network::bind(eth_initiator_socket& tx, eth_target_socket& rx) {
    while (eth_tx.exists(m_next_id) || eth_rx.exists(m_next_id))
        m_next_id++;

    eth_tx[m_next_id].bind(rx);
    tx.bind(eth_rx[m_next_id]);
    m_next_id++;
}

VCML_EXPORT_MODEL(vcml::ethernet::network, name, args) {
    return new network(name);
}

} // namespace ethernet
} // namespace vcml
