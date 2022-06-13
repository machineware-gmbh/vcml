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

#ifndef VCML_CAN_BUS_H
#define VCML_CAN_BUS_H

#include "vcml/core/types.h"
#include "vcml/core/report.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"

#include "vcml/protocols/can.h"

namespace vcml {
namespace can {

class bus : public module, public can_host
{
protected:
    id_t m_next_id;

    const can_initiator_socket& peer_of(const can_target_socket& rx) const {
        return can_tx[can_rx.index_of(rx)];
    }

    void can_receive(const can_target_socket&, can_frame& frame) override;

public:
    can_initiator_socket_array<> can_tx;
    can_target_socket_array<> can_rx;

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
