/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/meta/gdbserver.h"

namespace vcml {
namespace meta {

void gdbserver::add_target(vcml::debugging::target& target) {
    VCML_ERROR_ON(m_gdb, "cannot add targets after elaboration");
    stl_add_unique(m_targets, &target);
}

void gdbserver::add_targets_from(const sc_module& parent) {
    for (auto* target : vcml::debugging::target::all()) {
        if (starts_with(target->target_name(), parent.name()))
            add_target(*target);
    }
}

gdbserver::gdbserver(const sc_module_name& nm,
                     const vector<vcml::debugging::target*>& targets):
    module(nm),
    m_targets(targets),
    m_gdb(),
    port("port", 0),
    host("host", "localhost"),
    wait("wait", port > 0),
    echo("echo", false) {
}

gdbserver::~gdbserver() {
    if (m_gdb)
        delete m_gdb;
}

void gdbserver::end_of_elaboration() {
    module::end_of_elaboration();

    if (port < 0)
        return;

    if (m_targets.empty()) {
        log_warn("no targets to debug, disabling GDB server");
        return;
    }

    try {
        auto state = wait ? vcml::debugging::GDB_STOPPED
                          : vcml::debugging::GDB_RUNNING;
        m_gdb = new vcml::debugging::gdbserver(host, port, m_targets, state);
        m_gdb->echo(echo);
        if (port == 0)
            port = m_gdb->port();

        log_info("%s for GDB connection on port %hu",
                 wait ? "waiting" : "listening", m_gdb->port());
    } catch (std::exception& ex) {
        VCML_REPORT("error starting gdbserver: %s", ex.what());
    }
}

VCML_EXPORT_MODEL(vcml::meta::gdbserver, name, args) {
    vector<vcml::debugging::target*> targets;
    for (const string& s : args) {
        auto* target = vcml::debugging::target::find(s);
        if (target)
            targets.push_back(target);
    }

    return new gdbserver(name, targets);
}

} // namespace meta
} // namespace vcml
