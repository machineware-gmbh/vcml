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

#include "vcml/models/generic/gate.h"

namespace vcml {
namespace generic {

static bool calc_and(const gpio_target_socket_array<>& in) {
    for (const auto& port : in)
        if (!port.second->read())
            return false;
    return true;
}

static bool calc_nand(const gpio_target_socket_array<>& in) {
    for (const auto& port : in)
        if (!port.second->read())
            return true;
    return false;
}

static bool calc_or(const gpio_target_socket_array<>& in) {
    for (const auto& port : in)
        if (port.second->read())
            return true;
    return false;
}

static bool calc_nor(const gpio_target_socket_array<>& in) {
    for (const auto& port : in)
        if (port.second->read())
            return false;
    return true;
}

gate::gate(const sc_module_name& nm, logic_type type):
    module(nm), in("in"), out("out"), m_type(type) {
    // nothing to do
}

const char* gate::kind() const {
    switch (m_type) {
    case LOGIC_NOT:
        return "gate_not";
    case LOGIC_AND:
        return "gate_and";
    case LOGIC_OR:
        return "gate_or";
    case LOGIC_NAND:
        return "gate_nand";
    case LOGIC_NOR:
        return "gate_nor";
    default:
        return "gate";
    }
}

void gate::gpio_transport(const gpio_target_socket& socket, gpio_payload& tx) {
    gpio_payload txout;
    txout.state = tx.state;
    txout.vector = tx.vector;

    switch (m_type) {
    case LOGIC_NOT:
        txout.state = !tx.state;
        break;
    case LOGIC_AND:
        txout.state = calc_and(in);
        break;
    case LOGIC_OR:
        txout.state = calc_or(in);
        break;
    case LOGIC_NAND:
        txout.state = calc_nand(in);
        break;
    case LOGIC_NOR:
        txout.state = calc_nor(in);
        break;
    default:
        break;
    }

    out->gpio_transport(txout);
}

} // namespace generic
} // namespace vcml
