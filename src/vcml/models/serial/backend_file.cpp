/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/serial/backend_file.h"
#include "vcml/models/serial/terminal.h"

namespace vcml {
namespace serial {

backend_file::backend_file(terminal* term, const string& rx, const string& tx):
    backend(term, mkstr("file:%s:%s", rx.c_str(), tx.c_str())),
    m_rx(),
    m_tx() {
    if (!rx.empty()) {
        m_rx.open(rx.c_str(), ifstream::binary | ifstream::in);
        if (!m_rx.good())
            log_warn("failed to open file '%s'", rx.c_str());
    }

    if (!tx.empty()) {
        auto mode = ofstream::binary | ofstream::app | ofstream::out;
        m_tx.open(tx.c_str(), mode);
        if (!m_tx.good())
            log_warn("failed to open file '%s'", tx.c_str());
    }
}

backend_file::~backend_file() {
    // nothing to do
}

bool backend_file::read(u8& val) {
    if (!m_rx.is_open() || !m_rx.good())
        return false;

    m_rx.read(reinterpret_cast<char*>(&val), sizeof(val));
    return m_rx.gcount() > 0u;
}

void backend_file::write(u8 val) {
    if (m_tx.is_open() && m_tx.good()) {
        m_tx.write(reinterpret_cast<const char*>(&val), sizeof(val));
        m_tx.flush();
    }
}

backend* backend_file::create(terminal* term, const string& type) {
    string rx = mkstr("%s.rx", term->name());
    string tx = mkstr("%s.tx", term->name());

    vector<string> args = split(type, ':');

    if (args.size() == 2) {
        rx = args[1] + ".rx";
        tx = args[1] + ".tx";
    }

    if (args.size() >= 3) {
        rx = args[1];
        tx = args[2];
    }

    return new backend_file(term, rx, tx);
}

} // namespace serial
} // namespace vcml
