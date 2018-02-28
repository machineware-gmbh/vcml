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

#ifndef VCML_COMPONENT_H
#define VCML_COMPONENT_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"

#include "vcml/range.h"
#include "vcml/txext.h"
#include "vcml/exmon.h"
#include "vcml/dmi_cache.h"
#include "vcml/command.h"

namespace vcml {

    class master_socket;
    class slave_socket;

    class component: public sc_module
    {
        friend class master_socket;
        friend class slave_socket;
    private:
        std::map<sc_process_b*, sc_time> m_offsets;

        vector<master_socket*> m_master_sockets;
        vector<slave_socket*> m_slave_sockets;

        void register_socket(master_socket* socket);
        void register_socket(slave_socket* socket);

        void unregister_socket(master_socket* socket);
        void unregister_socket(slave_socket* socket);

        std::map<string, command_base*> m_commands;

        bool cmd_clist(const vector<string>& args, ostream& os);
        bool cmd_cinfo(const vector<string>& args, ostream& os);
        bool cmd_reset(const vector<string>& args, ostream& os);

        void do_reset();

    public:
        property<bool> allow_dmi;

        component(const sc_module_name& nm, bool allow_dmi = true);
        virtual ~component();

        VCML_KIND(component);

        virtual void reset();

        const sc_time& get_offset(sc_process_b* proc = NULL) const;
        sc_time& offset(sc_process_b* proc = NULL);
        void sync(sc_process_b* proc = NULL);

        void hierarchy_push();
        void hierarchy_pop();

        const vector<master_socket*>& get_master_sockets() const;
        const vector<slave_socket*>& get_slave_sockets() const;

        master_socket* get_master_socket(const string& name) const;
        slave_socket* get_slave_socket(const string& name) const;

        void map_dmi(const tlm_dmi& dmi);
        void map_dmi(unsigned char* ptr, u64 start, u64 end, vcml_access a,
                     const sc_time& read_latency = SC_ZERO_TIME,
                     const sc_time& write_latency = SC_ZERO_TIME);
        void unmap_dmi(const tlm_dmi& dmi);
        void unmap_dmi(u64 start, u64 end);

        bool execute(const string& name, const vector<string>& args,
                     ostream& os);

        template <class T>
        void register_command(const string& name, unsigned int argc,
                    T* host, bool (T::*func)(const vector<string>&, ostream&),
                    const string& description);

        command_base* get_command(const string& name);
        vector<command_base*> get_commands() const;

        virtual void b_transport(slave_socket* origin, tlm_generic_payload& tx,
                                         sc_time& dt);
        virtual unsigned int transport_dbg(slave_socket* origin,
                                           tlm_generic_payload& tx);
        virtual bool get_direct_mem_ptr(slave_socket* origin,
                                        const tlm_generic_payload& tx,
                                        tlm_dmi& dmi);
        virtual void invalidate_direct_mem_ptr(master_socket* origin,
                                               u64 start, u64 end);

        virtual unsigned int transport(tlm_generic_payload& tx, sc_time& dt,
                                       int flags);
        virtual void invalidate_dmi(u64 start, u64 end);
    };

    inline sc_time& component::offset(sc_process_b* proc) {
        return m_offsets[proc ? proc : sc_get_current_process_b()];
    }

    inline const sc_time& component::get_offset(sc_process_b* proc) const {
        if (proc == NULL)
            proc = sc_get_current_process_b();
        if (!stl_contains(m_offsets, proc))
            return SC_ZERO_TIME;
        return m_offsets.at(proc);
    }

    inline void component::sync(sc_process_b* proc) {
        if (proc == NULL)
            proc = sc_get_current_process_b();
        if (proc == NULL || proc->proc_kind() != sc_core::SC_THREAD_PROC_)
            VCML_ERROR("attempt to sync outside of SC_THREAD process");

        sc_time& to = offset(proc);
        if (to != SC_ZERO_TIME) {
            wait(to);
            to = SC_ZERO_TIME;
        }
    }

    inline void component::hierarchy_push() {
        sc_simcontext* simc = sc_get_curr_simcontext();
        VCML_ERROR_ON(!simc, "no simulation context");
        simc->hierarchy_push(this);
    }

    inline void component::hierarchy_pop() {
        sc_simcontext* simc = sc_get_curr_simcontext();
        VCML_ERROR_ON(!simc, "no simulation context");
        sc_module* top = simc->hierarchy_pop();
        VCML_ERROR_ON(top != this, "broken hierarchy");
    }

    inline const vector<master_socket*>& component::get_master_sockets() const {
        return m_master_sockets;
    }

    inline const vector<slave_socket*>& component::get_slave_sockets() const {
        return m_slave_sockets;
    }

    inline void component::unmap_dmi(const tlm_dmi& dmi) {
        unmap_dmi(dmi.get_start_address(), dmi.get_end_address());
    }

    template <class T>
    void component::register_command(const string& name, unsigned int argc,
                T* host, bool (T::*func)(const vector<string>&, ostream&),
                const string& desc) {
        if (stl_contains(m_commands, name))
            VCML_ERROR("command '%s' already registered", name.c_str());
        m_commands[name] = new command<T>(name, argc, desc, host, func);
    }

    inline command_base* component::get_command(const string& name) {
        return m_commands[name];
    }

}

#endif
