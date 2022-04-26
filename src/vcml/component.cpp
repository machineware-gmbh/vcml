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
    reset();
    for (auto obj : get_child_objects()) {
        component* child = dynamic_cast<component*>(obj);
        if (child)
            child->reset();
    }
}

void component::clock_handler() {
    clock_t newclk = clk.read();
    if (newclk == m_curclk)
        return;

    if (newclk <= 0) {
        for (auto socket : get_tlm_target_sockets())
            socket->invalidate_dmi();
    }

    log_debug("changed clock from %ldHz to %ldHz", m_curclk, newclk);
    handle_clock_update(m_curclk, newclk);

    m_curclk = newclk;
}

void component::reset_handler() {
    do_reset();
}

component::component(const sc_module_name& nm, bool dmi):
    module(nm), tlm_host(dmi), m_curclk(), clk("clk"), rst("rst") {
    SC_HAS_PROCESS(component);
    SC_METHOD(clock_handler);
    sensitive << clk;
    dont_initialize();

    SC_METHOD(reset_handler);
    sensitive << rst.pos();
    dont_initialize();

    register_command("reset", 0, this, &component::cmd_reset,
                     "resets this component");
}

component::~component() {
    // nothing to do
}

void component::reset() {
    for (auto socket : get_tlm_target_sockets())
        socket->invalidate_dmi();
}

void component::wait_clock_reset() {
    if (!is_thread())
        return;

    while (clk <= 0 || rst) {
        if (clk <= 0)
            wait(clk.default_event());
        if (rst)
            wait(rst.negedge_event());
    }
}

void component::wait_clock_cycle() {
    wait_clock_reset();
    wait(clock_cycle());
}

void component::wait_clock_cycles(u64 num) {
    for (u64 i = 0; i < num; i++)
        wait_clock_cycle();
}

sc_time component::clock_cycle() const {
    if (!clk.is_bound() || clk <= 0)
        return SC_ZERO_TIME;
    return sc_time(1.0 / clk.read(), SC_SEC);
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

void component::end_of_elaboration() {
    module::end_of_elaboration();
    reset();
}

} // namespace vcml
