/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/serial/terminal.h"
#include "vcml/models/serial/backend_tcp.h"

namespace vcml {
namespace serial {

void backend_tcp::iothread() {
    mwr::set_thread_name(mkstr("serial_%hu", m_socket.port()));

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

    while (m_socket.is_connected()) {
        u8 data;
        m_socket.recv(data);
        m_mtx.lock();
        m_fifo.push(data);
        m_mtx.unlock();
        m_term->notify(this);
    }
}

backend_tcp::backend_tcp(terminal* term, u16 port):
    backend(term, "tcp"),
    m_socket(port),
    m_thread(),
    m_mtx(),
    m_fifo(),
    m_running(true) {
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

bool backend_tcp::read(u8& val) {
    lock_guard<mutex> guard(m_mtx);
    if (m_fifo.empty())
        return false;

    val = m_fifo.front();
    m_fifo.pop();
    return true;
}

void backend_tcp::write(u8 val) {
    try {
        if (m_socket.is_connected())
            m_socket.send(val);
    } catch (...) {
        // nothing to do
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
