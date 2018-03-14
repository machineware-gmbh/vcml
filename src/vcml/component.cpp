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
        std::map<string, command_base*>::iterator it;
        for (it = m_commands.begin(); it != m_commands.end(); it++)
            os << it->first << ",";
        return true;
    }

    bool component::cmd_cinfo(const vector<string>& args, ostream& os) {
        command_base* cmd = m_commands[args[0]];
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

    void component::do_reset() {
        reset();
        for (auto obj : get_child_objects()) {
            component* child = dynamic_cast<component*>(obj);
            if (child)
                child->reset();
        }
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

    component::component(const sc_module_name& nm, bool dmi):
        sc_module(nm),
        m_offsets(),
        m_master_sockets(),
        m_slave_sockets(),
        m_commands(),
        allow_dmi("allow_dmi", dmi),
        loglvl("loglvl", default_log_level()) {
        register_command("clist", 0, this, &component::cmd_clist,
                         "returns a list of supported commands");
        register_command("cinfo", 1, this, &component::cmd_cinfo,
                         "returns information on a given command");
        register_command("reset", 0 ,this, &component::cmd_reset,
                         "resets this component");
    }

    component::~component() {
        // nothing to do
    }

    void component::reset() {
        // nothing to do
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

    bool component::execute(const string& name, const vector<string>& args,
                            ostream& os) {
        command_base* cmd = m_commands[name];
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
            list.push_back(cmd.second);
        return list;
    }

    void component::b_transport(slave_socket* origin, tlm_generic_payload& tx,
                                sc_time& dt) {
        trace_in(tx);
        transport(tx, dt, tx_is_excl(tx) ? VCML_FLAG_EXCL : VCML_FLAG_NONE);
        trace_out(tx);
    }

    unsigned int component::transport_dbg(slave_socket* origin,
                                          tlm_generic_payload& tx) {
        sc_time zero = SC_ZERO_TIME;
        sc_time t1 = sc_time_stamp();
        unsigned int bytes = transport(tx, zero, VCML_FLAG_DEBUG);
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
                                      int flags) {
        return 0; /* to be overloaded */
    }

    void component::invalidate_dmi(u64 start, u64 end) {
        /* to be overloaded */
    }

}
