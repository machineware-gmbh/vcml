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

    while (m_running) {
        try {
            receive();
        } catch (...) {
            // ignored
        }
    }
}

void backend_tcp::receive() {
    if (!m_socket.accept())
        return;

    while (m_socket.is_connected() && sim_running()) {
        if (m_socket.peek(100)) {
            can_frame frame;
            m_socket.recv(frame);
            send_to_guest(frame);
        }
    }
}

backend_tcp::backend_tcp(bridge* br, u16 port):
    backend(br), m_socket(port), m_thread(), m_running(true) {
    m_type = mkstr("tcp:%hu", m_socket.port());
    m_thread = std::thread(&backend_tcp::iothread, this);
    log_info("listening on port %hu", m_socket.port());
}

backend_tcp::~backend_tcp() {
    m_running = false;

    if (m_socket.is_listening())
        m_socket.unlisten();

    if (m_socket.is_connected())
        m_socket.disconnect();

    if (m_thread.joinable())
        m_thread.join();
}

void backend_tcp::send_to_host(const can_frame& frame) {
    try {
        if (m_socket.is_connected())
            m_socket.send(frame);
    } catch (...) {
        // nothing to do
    }
}

backend* backend_tcp::create(bridge* br, const string& type) {
    vector<string> args = split(type, ':');

    u16 port = 0;
    if (args.size() > 1)
        port = from_string<u16>(args[1]);

    return new backend_tcp(br, port);
}

} // namespace can
} // namespace vcml
