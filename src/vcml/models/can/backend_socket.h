/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_CAN_BACKEND_SOCKET_H
#define VCML_CAN_BACKEND_SOCKET_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/logging/logger.h"

#include "vcml/models/can/backend.h"
#include "vcml/models/can/bridge.h"

namespace vcml {
namespace can {

class backend_socket : public backend
{
private:
    string m_name;
    int m_socket;

public:
    backend_socket(bridge* br, const string& ifname);
    virtual ~backend_socket();

    virtual void send_to_host(const can_frame& frame) override;

    static backend* create(bridge* br, const string& type);
};

} // namespace can
} // namespace vcml

#endif
