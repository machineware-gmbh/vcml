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

#include "vcml/common/version.h"

#include "vcml/module.h"

namespace vcml {

bool module::cmd_clist(const vector<string>& args, ostream& os) {
    for (const auto& cmd : m_commands)
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

bool module::cmd_version(const vector<string>& args, ostream& os) {
    os << kind() << " " << version();
    return true;
}

module::module(const sc_module_name& nm):
    sc_module(nm),
    m_commands(),
    trace("trace", false),
    trace_errors("trace_errors", false),
    loglvl("loglvl", LOG_INFO),
    log(this) {
    trace.inherit_default();
    trace_errors.inherit_default();
    loglvl.inherit_default();
    register_command("clist", 0, this, &module::cmd_clist,
                     "returns a list of supported commands");
    register_command("cinfo", 1, this, &module::cmd_cinfo,
                     "returns information on a given command");
    register_command("abort", 0, this, &module::cmd_abort,
                     "immediately aborts the simulation");
    register_command("version", 0, this, &module::cmd_version,
                     "print version information about this module");
}

module::~module() {
    for (const auto& it : m_commands)
        delete it.second;
}

const char* module::version() const {
    return VCML_VERSION_STRING;
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
    for (const auto& cmd : m_commands)
        if (cmd.second != nullptr)
            list.push_back(cmd.second);
    return list;
}

} // namespace vcml
