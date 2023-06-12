/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_ETHERNET_BACKEND_TAP_H
#define VCML_ETHERNET_BACKEND_TAP_H

#include "vcml/core/types.h"
#include "vcml/logging/logger.h"

#include "vcml/models/ethernet/backend.h"
#include "vcml/models/ethernet/bridge.h"

namespace vcml {
namespace ethernet {

class backend_tap : public backend
{
private:
    int m_fd;

    void close_tap();

public:
    backend_tap(bridge* br, int devno);
    virtual ~backend_tap();

    virtual void send_to_host(const eth_frame& frame) override;

    static backend* create(bridge* br, const string& type);
};

} // namespace ethernet
} // namespace vcml

#endif
