/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/generic/gate.h"

namespace vcml {
namespace generic {

static bool calc_and(const gpio_target_array& in) {
    for (const auto& port : in)
        if (!port.second->read())
            return false;
    return true;
}

static bool calc_nand(const gpio_target_array& in) {
    for (const auto& port : in)
        if (!port.second->read())
            return true;
    return false;
}

static bool calc_or(const gpio_target_array& in) {
    for (const auto& port : in)
        if (port.second->read())
            return true;
    return false;
}

static bool calc_nor(const gpio_target_array& in) {
    for (const auto& port : in)
        if (port.second->read())
            return false;
    return true;
}

static bool calc_xor(const gpio_target_array& in) {
    bool result = false;
    for (const auto& port : in)
        result ^= port.second->read();
    return result;
}

gate::gate(const sc_module_name& nm, logic_type type):
    module(nm), in("in"), out("out"), m_type(type) {
    // nothing to do
}

const char* gate::kind() const {
    switch (m_type) {
    case LOGIC_NOT:
        return "vcml::gate_not";
    case LOGIC_AND:
        return "vcml::gate_and";
    case LOGIC_OR:
        return "vcml::gate_or";
    case LOGIC_NAND:
        return "vcml::gate_nand";
    case LOGIC_NOR:
        return "vcml::gate_nor";
    case LOGIC_XOR:
        return "vcml::gate_xor";
    default:
        return "vcml::gate";
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
    case LOGIC_XOR:
        txout.state = calc_xor(in);
        break;
    default:
        break;
    }

    out->gpio_transport(txout);
}

VCML_EXPORT_MODEL(vcml::generic::not_gate, name, args) {
    return new gate(name, gate::LOGIC_NOT);
}

VCML_EXPORT_MODEL(vcml::generic::and_gate, name, args) {
    return new gate(name, gate::LOGIC_AND);
}

VCML_EXPORT_MODEL(vcml::generic::or_gate, name, args) {
    return new gate(name, gate::LOGIC_OR);
}

VCML_EXPORT_MODEL(vcml::generic::nand_gate, name, args) {
    return new gate(name, gate::LOGIC_NAND);
}

VCML_EXPORT_MODEL(vcml::generic::nor_gate, name, args) {
    return new gate(name, gate::LOGIC_NOR);
}

VCML_EXPORT_MODEL(vcml::generic::xor_gate, name, args) {
    return new gate(name, gate::LOGIC_XOR);
}

} // namespace generic
} // namespace vcml
