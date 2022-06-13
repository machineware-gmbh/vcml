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

#ifndef VCML_ETHERNET_NETWORK_H
#define VCML_ETHERNET_NETWORK_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/protocols/eth.h"

#include "vcml/module.h"

namespace vcml {
namespace ethernet {

class network : public module, public eth_host
{
protected:
    id_t m_next_id;

    const eth_initiator_socket& peer_of(const eth_target_socket& rx) const {
        return eth_tx[eth_rx.index_of(rx)];
    }

    void eth_receive(const eth_target_socket&, eth_frame& frame) override;

public:
    eth_initiator_socket_array<> eth_tx;
    eth_target_socket_array<> eth_rx;

    network(const sc_module_name& nm);
    virtual ~network() = default;
    VCML_KIND(ethernet::network);

    void bind(eth_initiator_socket& tx, eth_target_socket& rx);

    template <typename DEVICE>
    void connect(DEVICE& device) {
        bind(device.eth_tx, device.eth_rx);
    }
};

} // namespace ethernet
} // namespace vcml

#endif
