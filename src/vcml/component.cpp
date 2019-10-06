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
#include "vcml/master_socket.h"
#include "vcml/slave_socket.h"

namespace vcml {

    void component::register_socket(master_socket* socket) {
        if (stl_contains(m_master_sockets, socket))
            VCML_ERROR("socket '%s' already registered", socket->name());
        m_master_sockets.push_back(socket);
    }

    void component::register_socket(slave_socket* socket) {
        if (stl_contains(m_slave_sockets, socket))
            VCML_ERROR("socket '%s' already registered", socket->name());
        m_slave_sockets.push_back(socket);
    }

    void component::unregister_socket(master_socket* socket) {
        stl_remove_erase(m_master_sockets, socket);
    }

    void component::unregister_socket(slave_socket* socket) {
        stl_remove_erase(m_slave_sockets, socket);
    }

    bool component::cmd_clist(const vector<string>& args, ostream& os) {
        for (auto cmd : m_commands)
            os << cmd.first << ",";
        return true;
    }

    bool component::cmd_cinfo(const vector<string>& args, ostream& os) {
        command_base* cmd = get_command(args[0]);
        if (cmd == NULL) {
            os << "no such command: " << args[0];
            return false;
        }

        os << cmd->desc();
        return true;
    }

    bool component::cmd_reset(const vector<string>& args, ostream& os) {
        do_reset();
        os << "OK";
        return true;
    }

    bool component::cmd_abort(const vector<string>& args, ostream& os) {
        std::abort();
        return true;
    }

    log_level component::default_log_level() const {
        sc_object* obj = get_parent_object();
        while (obj != NULL) {
            component* comp = dynamic_cast<component*>(obj);
            if (comp)
                return comp->loglvl;
            obj = obj->get_parent_object();
        }

        return LOG_INFO;
    }

    void component::do_reset() {
        reset();
        for (auto obj : get_child_objects()) {
            component* child = dynamic_cast<component*>(obj);
            if (child)
                child->reset();
        }
    }

    SC_HAS_PROCESS(component);

    void component::clock_handler() {
        clock_t newclk = CLOCK.read();
        if (newclk == m_curclk)
            return;

        log_debug("changed clock from %ldHz to %ldHz", m_curclk, newclk);
        handle_clock_update(m_curclk, newclk);

        m_curclk = newclk;
    }

    void component::reset_handler() {
        do_reset();
    }

    component::component(const sc_module_name& nm, bool dmi):
        sc_module(nm),
        m_curclk(),
        m_offsets(),
        m_master_sockets(),
        m_slave_sockets(),
        m_commands(),
        allow_dmi("allow_dmi", dmi),
        trace_errors("trace_errors", false),
        loglvl("loglvl", trace_errors ? LOG_TRACE : default_log_level()),
        CLOCK("CLOCK"),
        RESET("RESET") {
        SC_METHOD(clock_handler);
        sensitive << CLOCK;
        dont_initialize();

        SC_METHOD(reset_handler);
        sensitive << RESET.pos();
        dont_initialize();

        register_command("clist", 0, this, &component::cmd_clist,
                         "returns a list of supported commands");
        register_command("cinfo", 1, this, &component::cmd_cinfo,
                         "returns information on a given command");
        register_command("reset", 0 ,this, &component::cmd_reset,
                         "resets this component");
        register_command("abort", 0, this, &component::cmd_abort,
                         "immediately aborts the simulation");
    }

    component::~component() {
        // nothing to do
    }

    void component::reset() {
        // nothing to do
    }

    void component::wait_clock_reset() {
        if (!is_thread())
            return;

        while (CLOCK <= 0 || RESET) {
            if (CLOCK <= 0)
                wait(CLOCK.default_event());
            if (RESET)
                wait(RESET.negedge_event());
        }
    }

    master_socket* component::get_master_socket(const string& name) const {
        for (auto socket : m_master_sockets)
            if (name == socket->name())
                return socket;
        return NULL;
    }

    slave_socket* component::get_slave_socket(const string& name) const {
        for (auto socket : m_slave_sockets)
            if (name == socket->name())
                return socket;
        return NULL;
    }

    void component::map_dmi(const tlm_dmi& dmi) {
        for (auto socket : m_slave_sockets)
            socket->map_dmi(dmi);
    }

    void component::map_dmi(unsigned char* p, u64 start, u64 end, vcml_access a,
                            const sc_time& read_latency,
                            const sc_time& write_latency) {
        tlm_dmi dmi;
        dmi.set_dmi_ptr(p);
        dmi.set_start_address(start);
        dmi.set_end_address(end);
        dmi.set_read_latency(read_latency);
        dmi.set_write_latency(write_latency);
        dmi_set_access(dmi, a);
        map_dmi(dmi);
    }

    void component::unmap_dmi(u64 start, u64 end) {
        for (auto socket : m_slave_sockets)
            socket->unmap_dmi(start, end);
    }

    void component::remap_dmi(const sc_time& rdlat, const sc_time& wrlat) {
        for (auto socket : m_slave_sockets)
            socket->remap_dmi(rdlat, wrlat);
    }

    bool component::execute(const string& name, const vector<string>& args,
                            ostream& os) {
        command_base* cmd = get_command(name);
        if (!cmd) {
            os << "command '" << name << "' not supported";
            return false;
        }

        if (args.size() < cmd->argc()) {
            os << "command '" << name << "' requires " << cmd->argc() << " "
               << "arguments (" << args.size() << " given)";
            return false;
        }

        return cmd->execute(args, os);
    }

    vector<command_base*> component::get_commands() const {
        vector<command_base*> list;
        for (auto cmd : m_commands)
            if (cmd.second != NULL)
                list.push_back(cmd.second);
        return list;
    }

    void component::b_transport(slave_socket* origin, tlm_generic_payload& tx,
                                sc_time& dt) {
        if (!trace_errors)
            trace_in(tx);

        wait_clock_reset();
        transport(tx, dt, tx_get_sbi(tx));

        if (!trace_errors || failed(tx.get_response_status()))
            trace_out(tx);
    }

    unsigned int component::transport_dbg(slave_socket* origin,
                                          tlm_generic_payload& tx) {
        sc_time zero = SC_ZERO_TIME;
        sc_time t1 = sc_time_stamp();
        unsigned int bytes = transport(tx, zero, tx_get_sbi(tx) | SBI_DEBUG);
        sc_time t2 = sc_time_stamp();
        VCML_ERROR_ON(t1 != t2, "time advance during debug call");
        return bytes;
    }

    bool component::get_direct_mem_ptr(slave_socket* origin,
                                       const tlm_generic_payload& tx,
                                       tlm_dmi& dmi) {
        return true;
    }

    void component::invalidate_direct_mem_ptr(master_socket* origin,
                                             u64 start, u64 end) {
        invalidate_dmi(start, end);
    }

    unsigned int component::transport(tlm_generic_payload& tx, sc_time& dt,
                                      const sideband& info) {
        return 0; // to be overloaded
    }

    void component::invalidate_dmi(u64 start, u64 end) {
        // to be overloaded
    }

    void component::handle_clock_update(clock_t oldclk, clock_t newclk) {
        // to be overloaded
    }

}
