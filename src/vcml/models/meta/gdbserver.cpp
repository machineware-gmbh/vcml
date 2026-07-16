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

module& gdbserver::find_parent() {
    module* m = dynamic_cast<module*>(get_parent_object());
    return m ? *m : *this;
}

gdbserver::gdbserver(const sc_module_name& nm,
                     vector<vcml::debugging::target*> gdb_targets):
    module(nm),
    m_parent(find_parent()),
    m_gdb(),
    targets(std::move(gdb_targets)),
    gdb_port(&m_parent, "gdb_port", 0),
    gdb_host(&m_parent, "gdb_host", "localhost"),
    gdb_wait(&m_parent, "gdb_wait", gdb_port > 0),
    gdb_echo(&m_parent, "gdb_echo", false) {
}

gdbserver::~gdbserver() {
    if (m_gdb)
        delete m_gdb;
}

void gdbserver::end_of_elaboration() {
    module::end_of_elaboration();

    if (gdb_port < 0)
        return;

    if (targets.empty())
        targets = vcml::debugging::target::all();
    if (targets.empty()) {
        log_debug("no targets to debug, disabling GDB server");
        return;
    }

    try {
        auto wait = gdb_wait ? vcml::debugging::GDB_STOPPED
                             : vcml::debugging::GDB_RUNNING;
        m_gdb = new vcml::debugging::gdbserver(gdb_host, gdb_port, targets,
                                               wait);
        m_gdb->echo(gdb_echo);
        if (gdb_port == 0)
            gdb_port = m_gdb->port();

        m_parent.log.info("%s for GDB connection on port %hu",
                          gdb_wait ? "waiting" : "listening", m_gdb->port());
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
