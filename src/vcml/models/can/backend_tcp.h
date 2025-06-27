/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_CAN_BACKEND_TCP_H
#define VCML_CAN_BACKEND_TCP_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/logging/logger.h"

#include "vcml/models/can/backend.h"
#include "vcml/models/can/bridge.h"

namespace vcml {
namespace can {

class backend_tcp : public backend
{
private:
    mwr::socket m_socket;

    thread m_thread;
    atomic<bool> m_running;

    void iothread();
    void receive();

public:
    u16 port() const { return m_socket.port(); }

    backend_tcp(bridge* br, u16 port);
    virtual ~backend_tcp();

    virtual void send_to_host(const can_frame& frame) override;

    static backend* create(bridge* br, const string& type);
};

} // namespace can
} // namespace vcml

#endif
