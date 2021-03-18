/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#include "vcml/backends/backend_tcp.h"

namespace vcml {

    backend_tcp::backend_tcp(const sc_module_name& nm, u16 p):
        backend(nm),
        m_sock(),
        port("port", p) {
        m_sock.listen(port);
        if (port == 0)
            port = m_sock.port();
        log_info("listening on port %hu", m_sock.port());
        m_sock.accept_async();
    }

    backend_tcp::~backend_tcp() {
        // nothing to do
    }

    size_t backend_tcp::peek() {
        try {
            return m_sock.peek();
        } catch (...) {
            m_sock.accept_async();
            return 0;
        }
    }

    size_t backend_tcp::read(void* buf, size_t len) {
        try {
            if (!m_sock.is_connected())
                return 0;
            m_sock.recv(buf, len);
            return len;
        } catch (...) {
            m_sock.accept_async();
            return 0;
        }
    }

    size_t backend_tcp::write(const void* buf, size_t len) {
        try {
            if (!m_sock.is_connected())
                return 0;
            m_sock.send(buf, len);
            return len;
        } catch (...) {
            m_sock.accept_async();
            return 0;
        }
    }

    backend* backend_tcp::create(const string& nm) {
        return new backend_tcp(nm.c_str());
    }

}
