/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_ETHERNET_BACKEND_FILE_H
#define VCML_ETHERNET_BACKEND_FILE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/logging/logger.h"

#include "vcml/models/ethernet/backend.h"
#include "vcml/models/ethernet/bridge.h"

namespace vcml {
namespace ethernet {

class backend_file : public backend
{
private:
    size_t m_count;
    ofstream m_tx;

public:
    backend_file(bridge* br, const string& tx);
    virtual ~backend_file();

    virtual void send_to_host(const eth_frame& frame) override;

    static backend* create(bridge* gw, const string& type);
};

} // namespace ethernet
} // namespace vcml

#endif
