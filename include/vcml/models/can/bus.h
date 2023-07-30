/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_CAN_BUS_H
#define VCML_CAN_BUS_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/can.h"

namespace vcml {
namespace can {

class bus : public module, public can_host
{
protected:
    size_t m_next_id;

    const can_initiator_socket& peer_of(const can_target_socket& rx) const {
        return can_tx[can_rx.index_of(rx)];
    }

    void can_receive(const can_target_socket&, can_frame& frame) override;

public:
    can_initiator_array can_tx;
    can_target_array can_rx;

    bus(const sc_module_name& nm);
    virtual ~bus() = default;
    VCML_KIND(can::bus);

    void bind(can_initiator_socket& tx, can_target_socket& rx);

    template <typename DEVICE>
    void connect(DEVICE& device) {
        bind(device.can_tx, device.can_rx);
    }
};

} // namespace can
} // namespace vcml

#endif
