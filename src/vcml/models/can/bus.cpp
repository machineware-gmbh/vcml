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

} // namespace can
} // namespace vcml
