/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/component.h"

namespace vcml {

bool component::cmd_reset(const vector<string>& args, ostream& os) {
    do_reset();
    os << "OK";
    return true;
}

void component::do_reset() {
    for (auto socket : get_tlm_target_sockets())
        socket->invalidate_dmi();

    reset();
}

component::component(const sc_module_name& nm, bool dmi, unsigned int bus):
    module(nm),
    tlm_host(dmi, bus),
    clk_host(),
    gpio_host(),
    m_clkrst_ev("clkrst_ev"),
    clk("clk"),
    rst("rst") {
    register_command("reset", 0, &component::cmd_reset,
                     "resets this component");
}

component::~component() {
    // nothing to do
}

void component::reset() {
    // to be overloaded
}

void component::wait_clock_reset() {
    if (!is_thread())
        return;

    while (clk == 0 || rst)
        wait(m_clkrst_ev);
}

void component::wait_clock_cycle() {
    wait_clock_reset();
    wait(clock_cycle());
}

void component::wait_clock_cycles(u64 num) {
    for (u64 i = 0; i < num; i++)
        wait_clock_cycle();
}

unsigned int component::transport(tlm_target_socket& socket,
                                  tlm_generic_payload& tx,
                                  const tlm_sbi& sideband) {
    if (!sideband.is_debug)
        wait_clock_reset();
    return transport(tx, sideband, socket.as);
}

unsigned int component::transport(tlm_generic_payload& tx,
                                  const tlm_sbi& sideband, address_space as) {
    // to be overloaded
    return 0;
}

void component::handle_clock_update(hz_t oldclk, hz_t newclk) {
    // to be overloaded
}

void component::clk_notify(const clk_target_socket& s, const clk_payload& tx) {
    log_debug("changed clock from %zuHz to %zuHz", tx.oldhz, tx.newhz);
    handle_clock_update(tx.oldhz, tx.newhz);
    m_clkrst_ev.notify(SC_ZERO_TIME);
}

void component::gpio_transport(const gpio_target_socket& socket,
                               gpio_payload& tx) {
    if (socket == rst) {
        if (tx.state)
            do_reset();
        m_clkrst_ev.notify(SC_ZERO_TIME);
    } else {
        gpio_notify(socket, tx.state, tx.vector);
    }
}

void component::gpio_notify(const gpio_target_socket& socket, bool state,
                            gpio_vector vector) {
    gpio_notify(socket, state);
}

void component::gpio_notify(const gpio_target_socket& socket, bool state) {
    gpio_notify(socket);
}

void component::gpio_notify(const gpio_target_socket& socket) {
    // to be overloaded
}

} // namespace vcml
