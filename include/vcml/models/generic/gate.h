/******************************************************************************
 *                                                                            *
 * Copyright 2022 MachineWare GmbH                                            *
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

#ifndef VCML_GENERIC_GATE_H
#define VCML_GENERIC_GATE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"

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
    };

    gpio_target_socket_array<> in;
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
