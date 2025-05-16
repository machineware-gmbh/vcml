/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/lin/gateway.h"

namespace vcml {
namespace lin {

void gateway::can_receive(const can_target_socket& sock, can_frame& frame) {
    lin_payload tx{};
    tx.status = LIN_INCOMPLETE;
    tx.linid = frame.id();

    size_t len = min(tx.size(), frame.length());
    memcpy(tx.data, frame.data, len);

    lin_out.transport(tx);

    if (failed(tx)) {
        frame.msgid |= CAN_ERR;
        return;
    }

    frame.msgid &= ~CAN_ERR;
    memcpy(frame.data, tx.data, len);
}

gateway::gateway(const sc_module_name& nm):
    module(nm), can_host(), can_in("can_in"), lin_out("lin_out") {
    // nothing to do
}

VCML_EXPORT_MODEL(vcml::lin::gateway, name, args) {
    return new gateway(name);
}

} // namespace lin
} // namespace vcml
