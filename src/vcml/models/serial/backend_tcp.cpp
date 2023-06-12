/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/serial/backend_tcp.h"

namespace vcml {
namespace serial {

backend_tcp::backend_tcp(terminal* term, u16 port):
    backend(term, "tcp"), m_socket(port) {
    m_socket.accept_async();
    m_type = mkstr("tcp:%hu", m_socket.port());
    log_info("listening on port %hu", m_socket.port());
}

backend_tcp::~backend_tcp() {
    // nothing to do
}

bool backend_tcp::read(u8& val) {
    try {
        if (!m_socket.is_connected() || !m_socket.peek())
            return false;

        m_socket.recv(&val, sizeof(val));
        return true;
    } catch (...) {
        m_socket.accept_async();
        return false;
    }
}

void backend_tcp::write(u8 val) {
    try {
        if (m_socket.is_connected())
            m_socket.send(&val, sizeof(val));
    } catch (...) {
        m_socket.accept_async();
    }
}

backend* backend_tcp::create(terminal* term, const string& type) {
    vector<string> args = split(type, ':');

    u16 port = 0;
    if (args.size() > 1)
        port = from_string<u16>(args[1]);

    return new backend_tcp(term, port);
}

} // namespace serial
} // namespace vcml
