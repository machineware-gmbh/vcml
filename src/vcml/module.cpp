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

#include "vcml/module.h"

namespace vcml {

    bool module::cmd_clist(const vector<string>& args, ostream& os) {
        for (auto cmd : m_commands)
            os << cmd.first << ",";
        return true;
    }

    bool module::cmd_cinfo(const vector<string>& args, ostream& os) {
        command_base* cmd = get_command(args[0]);
        if (cmd == nullptr) {
            os << "no such command: " << args[0];
            return false;
        }

        os << cmd->desc();
        return true;
    }

    bool module::cmd_abort(const vector<string>& args, ostream& os) {
        std::abort();
        return true;
    }

    log_level module::default_log_level() const {
        sc_object* obj = get_parent_object();
        while (obj != nullptr) {
            module* comp = dynamic_cast<module*>(obj);
            if (comp)
                return comp->loglvl;
            obj = obj->get_parent_object();
        }

        return LOG_INFO;
    }

    module::module(const sc_module_name& nm):
        sc_module(nm),
        m_commands(),
        trace_errors("trace_errors", false),
        loglvl("loglvl", trace_errors ? LOG_TRACE : default_log_level()) {
        register_command("clist", 0, this, &module::cmd_clist,
                         "returns a list of supported commands");
        register_command("cinfo", 1, this, &module::cmd_cinfo,
                         "returns information on a given command");
        register_command("abort", 0, this, &module::cmd_abort,
                         "immediately aborts the simulation");
    }

    module::~module() {
        for (auto it : m_commands)
            delete it.second;
    }

    void module::session_suspend() {
        // to be overloaded
    }

    void module::session_resume() {
        // to be overloaded
    }

    bool module::execute(const string& name, const vector<string>& args,
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

    vector<command_base*> module::get_commands() const {
        vector<command_base*> list;
        for (auto cmd : m_commands)
            if (cmd.second != nullptr)
                list.push_back(cmd.second);
        return list;
    }

}
