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

        log_level default_log_level() const;

    public:
        property<bool> trace_errors;
        property<log_level> loglvl;

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

#ifndef VCML_OMIT_LOGGING_SOURCE
        void log_tagged(log_level lvl, const char* file, int line,
                        const char* format, ...) const VCML_DECL_PRINTF(5, 6) {
            if (lvl <= loglvl && logger::would_log(lvl)) {
                va_list args; va_start(args, format);
                logger::publish(lvl, name(), vmkstr(format, args), file, line);
                va_end(args);
            }
        }
#else
#define VCML_GEN_LOGFN(func, lvl)                                             \
        void func(const char* format, ...) const VCML_DECL_PRINTF(2, 3) {     \
            if (lvl <= loglvl && logger::would_log(lvl)) {                    \
                va_list args; va_start(args, format);                         \
                logger::publish(lvl, name(), vmkstr(format, args));           \
                va_end(args);                                                 \
            }                                                                 \
        }

        VCML_GEN_LOGFN(log_error, ::vcml::LOG_ERROR)
        VCML_GEN_LOGFN(log_warn, ::vcml::LOG_WARN)
        VCML_GEN_LOGFN(log_info, ::vcml::LOG_INFO)
        VCML_GEN_LOGFN(log_debug, ::vcml::LOG_DEBUG)
#undef VCML_GEN_LOGFN
#endif

        template <typename PAYLOAD>
        void trace_fw(const string& port, const PAYLOAD& tx,
                      const sc_time& dt = SC_ZERO_TIME);
        template <typename PAYLOAD>
        void trace_bw(const string& port, const PAYLOAD& tx,
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

    template <typename PAYLOAD>
    inline void module::trace_fw(const string& port, const PAYLOAD& tx,
                                 const sc_time& dt) {
        if (!trace_errors && loglvl >= LOG_TRACE)
            logger::trace_fw(port, tx, dt);
    }

    template <typename PAYLOAD>
    inline void module::trace_bw(const string& port, const PAYLOAD& tx,
                                 const sc_time& dt) {
        if ((!trace_errors || failed(tx)) && loglvl >= LOG_TRACE)
            logger::trace_bw(port, tx, dt);
    }

}

#endif

