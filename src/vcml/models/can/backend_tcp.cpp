/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/can/backend_tcp.h"

namespace vcml {
namespace can {

void backend_tcp::iothread() {
    mwr::set_thread_name(mkstr("canio_%hu", m_socket.port()));
    m_socket.set_tcp_nodelay(true);

    while (m_running) {
        try {
            int client = m_socket.poll(100);
            if (client >= 0) {
                can_frame frame;
                m_socket.recv(client, frame);
                send_to_guest(frame);
            }
        } catch (...) {
            // ignored
        }
    }
}

backend_tcp::backend_tcp(bridge* br, u16 port, const string& host):
    backend(br), m_socket(16, port, host), m_thread(), m_running(true) {
    m_type = mkstr("tcp:%hu", m_socket.port());
    m_thread = std::thread(&backend_tcp::iothread, this);
    log_info("listening on port %hu", m_socket.port());
}

backend_tcp::~backend_tcp() {
    m_running = false;

    m_socket.unlisten();
    m_socket.disconnect_all();

    if (m_thread.joinable())
        m_thread.join();
}

void backend_tcp::send_to_host(const can_frame& frame) {
    const auto& clients = m_socket.clients();
    for (int client : clients) {
        try {
            m_socket.send(client, frame);
        } catch (...) {
            // nothing to do
        }
    }
}

backend* backend_tcp::create(bridge* br, const vector<string>& args) {
    u16 port = 0;
    if (args.size() > 0)
        port = from_string<u16>(args[0]);

    string host = "localhost";
    if (args.size() > 1)
        host = args[1];

    return new backend_tcp(br, port, host);
}

} // namespace can
} // namespace vcml
