/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_LIN_GATEWAY_H
#define VCML_LIN_GATEWAY_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/can.h"
#include "vcml/protocols/lin.h"

namespace vcml {
namespace lin {

class gateway : public module, public can_host
{
protected:
    virtual void can_receive(const can_target_socket& sock,
                             can_frame& frame) override;

public:
    can_target_socket can_in;
    lin_initiator_socket lin_out;

    gateway(const sc_module_name& nm);
    virtual ~gateway() = default;
    VCML_KIND(lin::gateway);
};

} // namespace lin
} // namespace vcml

#endif
