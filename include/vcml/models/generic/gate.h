/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_GENERIC_GATE_H
#define VCML_GENERIC_GATE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/gpio.h"

namespace vcml {
namespace generic {

class gate : public module, public gpio_host
{
public:
    enum logic_type {
        LOGIC_NOT,
        LOGIC_AND,
        LOGIC_OR,
        LOGIC_NAND,
        LOGIC_NOR,
        LOGIC_XOR,
    };

    gpio_target_array in;
    gpio_initiator_socket out;

    gate(const sc_module_name& name, logic_type type);
    virtual ~gate() = default;
    virtual const char* kind() const override;

protected:
    logic_type m_type;

    virtual void gpio_transport(const gpio_target_socket& socket,
                                gpio_payload& tx) override;
};

} // namespace generic
} // namespace vcml

#endif
