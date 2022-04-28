/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#include "vcml/component.h"

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

component::component(const sc_module_name& nm, bool dmi):
    module(nm),
    tlm_host(dmi),
    rst_host(),
    clk_host(),
    m_clkrst_ev("clkrst_ev"),
    clk("clk"),
    rst("rst") {
    register_command("reset", 0, this, &component::cmd_reset,
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

void component::handle_clock_update(clock_t oldclk, clock_t newclk) {
    // to be overloaded
}

void component::clk_notify(const clk_target_socket& s, const clk_payload& tx) {
    log_debug("changed clock from %ldHz to %ldHz", tx.oldhz, tx.newhz);
    handle_clock_update(tx.oldhz, tx.newhz);
    m_clkrst_ev.notify(SC_ZERO_TIME);
}

void component::rst_notify(const rst_target_socket& s, const rst_payload& tx) {
    if (tx.reset)
        do_reset();
    m_clkrst_ev.notify(SC_ZERO_TIME);
}

void component::end_of_elaboration() {
    module::end_of_elaboration();
    reset();
}

} // namespace vcml
