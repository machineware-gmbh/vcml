/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/version.h"
#include "vcml/core/module.h"

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

// clang-format-15 seems to get confused when your class is named 'module', so
// we need to disable formatting here. If we rename this to module1::module1,
// no errors are reported. If we rename it back, the errors return...
// clang-format off
module::module(const sc_module_name& nm):
    sc_module(nm),
    hierarchy_element(),
    m_commands(),
    trace_all("trace", false),
    trace_errors("trace_errors", false),
    loglvl("loglvl", LOG_INFO),
    log(this) {
    trace_all.inherit_default();
    trace_errors.inherit_default();
    loglvl.inherit_default();
    register_command("clist", 0, &module::cmd_clist,
                     "returns a list of supported commands");
    register_command("cinfo", 1, &module::cmd_cinfo,
                     "returns information on a given command");
    register_command("abort", 0, &module::cmd_abort,
                     "immediately aborts the simulation");
    register_command("version", 0, &module::cmd_version,
                     "print version information about this module");
}
// clang-format on

module::~module() {
    for (const auto& it : m_commands)
        delete it.second;
}

const char* module::version() const {
    module* parent = dynamic_cast<module*>(get_parent_object());
    return parent ? parent->version() : VCML_VERSION_STRING;
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
