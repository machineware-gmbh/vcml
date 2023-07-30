/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_ETHERNET_NETWORK_H
#define VCML_ETHERNET_NETWORK_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/eth.h"

namespace vcml {
namespace ethernet {

class network : public module, public eth_host
{
protected:
    size_t m_next_id;

    const eth_initiator_socket& peer_of(const eth_target_socket& rx) const {
        return eth_tx[eth_rx.index_of(rx)];
    }

    void eth_receive(const eth_target_socket&, eth_frame& frame) override;

public:
    eth_initiator_array eth_tx;
    eth_target_array eth_rx;

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
