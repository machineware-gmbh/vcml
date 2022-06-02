/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include "vcml/ethernet/network.h"

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

} // namespace ethernet
} // namespace vcml
