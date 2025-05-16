/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_LIN_NETWORK_H
#define VCML_LIN_NETWORK_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/lin.h"

namespace vcml {
namespace lin {

class network : public module, public lin_host
{
protected:
    size_t m_next_id;

    void lin_receive(const lin_target_socket&, lin_payload&) override;

public:
    lin_initiator_array<> lin_out;
    lin_target_socket lin_in;

    network(const sc_module_name& nm);
    virtual ~network() = default;
    VCML_KIND(lin::network);

    void bind(lin_initiator_socket& socket);
    void bind(lin_target_socket& socket);
};

} // namespace lin
} // namespace vcml

#endif
