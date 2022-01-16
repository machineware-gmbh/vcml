/******************************************************************************
 *                                                                            *
 * Copyright 2019 Jan Henrik Weinstock                                        *
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

#ifndef VCML_MODULE_H
#define VCML_MODULE_H

#include "vcml/common/types.h"
#include "vcml/common/strings.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"

#include "vcml/command.h"

namespace vcml {

    class module: public sc_module
    {
    private:
        std::map<string, command_base*> m_commands;

        bool cmd_clist(const vector<string>& args, ostream& os);
        bool cmd_cinfo(const vector<string>& args, ostream& os);
        bool cmd_abort(const vector<string>& args, ostream& os);

    public:
        property<bool> trace_errors;
        property<log_level> loglvl;

        logger log;

        module() = delete;
        module(const module&) = delete;
        module(const sc_module_name& nm);
        virtual ~module();
        VCML_KIND(module);

        void hierarchy_push();
        void hierarchy_pop();

        virtual void session_suspend();
        virtual void session_resume();

        bool execute(const string& name, const vector<string>& args,
                     ostream& os);

        template <class T>
        void register_command(const string& name, unsigned int argc,
                    T* host, bool (T::*func)(const vector<string>&, ostream&),
                    const string& description);

        command_base* get_command(const string& name);
        vector<command_base*> get_commands() const;

        template <typename PORT, typename PAYLOAD>
        void trace(trace_direction dir, const PORT& port, const PAYLOAD& tx,
                   const sc_time& dt = SC_ZERO_TIME);
        template <typename PORT, typename PAYLOAD>
        void trace_fw(const PORT& port, const PAYLOAD& tx,
                      const sc_time& dt = SC_ZERO_TIME);
        template <typename PORT, typename PAYLOAD>
        void trace_bw(const PORT& port, const PAYLOAD& tx,
                      const sc_time& dt = SC_ZERO_TIME);
    };

    inline void module::hierarchy_push() {
        vcml::hierarchy_push(this);
    }

    inline void module::hierarchy_pop() {
        sc_module* top = vcml::hierarchy_pop();
        VCML_ERROR_ON(top != this, "broken hierarchy");
    }

    template <class T>
    void module::register_command(const string& cmdnm, unsigned int argc,
                T* host, bool (T::*func)(const vector<string>&, ostream&),
                const string& desc) {
        if (stl_contains(m_commands, cmdnm)) {
            VCML_ERROR("module %s already has a command called %s",
                       name(), cmdnm.c_str());
        }

        m_commands[cmdnm] = new command<T>(cmdnm, argc, desc, host, func);
    }

    inline command_base* module::get_command(const string& name) {
        if (!stl_contains(m_commands, name))
            return nullptr;
        return m_commands[name];
    }

    template <typename PORT, typename PAYLOAD>
    inline void module::trace(trace_direction dir, const PORT& port,
                              const PAYLOAD& tx, const sc_time& dt) {
        if (loglvl < LOG_TRACE || !publisher::would_publish(LOG_TRACE))
            return;

        if (trace_errors) {
            if (!failed(tx))
                return;
            dir = (dir != TRACE_FW) ? dir : TRACE_FW_NOINDENT;
            dir = (dir != TRACE_BW) ? dir : TRACE_BW_NOINDENT;
        }

        publisher::trace(dir, port, tx, dt);
    }

    template <typename PORT, typename PAYLOAD>
    inline void module::trace_fw(const PORT& port, const PAYLOAD& tx,
                                 const sc_time& dt) {
        trace(TRACE_FW, port, tx, dt);
    }

    template <typename PORT, typename PAYLOAD>
    inline void module::trace_bw(const PORT& port, const PAYLOAD& tx,
                                 const sc_time& dt) {
        trace(TRACE_BW, port, tx, dt);
    }

}

#endif

