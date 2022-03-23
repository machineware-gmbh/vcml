/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#include "vcml/serial/backend_tcp.h"

namespace vcml {
namespace serial {

backend_tcp::backend_tcp(const string& serial, int port):
    backend(serial), m_socket(port) {
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

backend* backend_tcp::create(const string& serial, const string& type) {
    int port            = 0;
    vector<string> args = split(type, ':');
    if (args.size() > 1)
        port = from_string<int>(args[1]);
    return new backend_tcp(serial, port);
}

} // namespace serial
} // namespace vcml
