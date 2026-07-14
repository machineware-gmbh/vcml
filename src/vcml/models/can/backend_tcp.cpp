/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/can/backend_tcp.h"

namespace vcml {
namespace can {

#pragma pack(push, 1)
struct frame_header {
    u32 msgid;
    u32 af;
    u8 flags;
    u8 sdt;
    u16 len;
};
#pragma pack(pop)

void backend_tcp::send_frame(int client, const can_frame& frame) {
    frame_header header{};
    header.msgid = mwr::cpu_to_le32(frame.msgid);
    header.af = mwr::cpu_to_le32(frame.af);
    header.flags = frame.flags;
    header.sdt = frame.sdt;
    header.len = mwr::cpu_to_le16(frame.length());

    m_socket.send(client, header);
    m_socket.send(client, frame.data.data(), frame.length());
}

void backend_tcp::receive_frame(int client, can_frame& frame) {
    frame_header header{};

    m_socket.recv(client, header);
    frame.msgid = mwr::le32_to_cpu(header.msgid);
    frame.af = mwr::le32_to_cpu(header.af);
    frame.flags = header.flags;
    frame.sdt = header.sdt;
    frame.data.resize(mwr::le16_to_cpu(header.len));

    m_socket.recv(client, frame.data.data(), frame.data.size());
}

void backend_tcp::iothread() {
    mwr::set_thread_name(mkstr("canio_%hu", m_socket.port()));
    m_socket.set_tcp_nodelay(true);

    while (m_running) {
        try {
            int client = m_socket.poll(100);
            if (client >= 0) {
                can_frame frame{};
                receive_frame(client, frame);
                send_to_guest(std::move(frame));
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
            send_frame(client, frame);
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
