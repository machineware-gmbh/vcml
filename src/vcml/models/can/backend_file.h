/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_CAN_BACKEND_FILE_H
#define VCML_CAN_BACKEND_FILE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/logging/logger.h"

#include "vcml/models/can/backend.h"
#include "vcml/models/can/bridge.h"

namespace vcml {
namespace can {

class backend_file : public backend
{
private:
    size_t m_count;
    ofstream m_tx;

public:
    backend_file(bridge* br, const string& tx);
    virtual ~backend_file();

    virtual void send_to_host(const can_frame& frame) override;

    static backend* create(bridge* br, const string& type);
};

} // namespace can
} // namespace vcml

#endif
