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

    void backend_tcp::handle_accept(int fd, int events) {
        if (fd != m_fd_server)
            return;

        listen();
    }

    u16 backend_tcp::default_port = 34444;

    backend_tcp::backend_tcp(const sc_module_name& nm, u16 p):
        backend(nm),
        m_fd(-1),
        m_fd_server(-1),
        m_server(),
        m_client(),
        port("port", p > 0 ? p : default_port++) {

        memset(&m_server, 0, sizeof(m_server));
        memset(&m_client, 0, sizeof(m_client));

        m_server.sin_family = AF_INET;
        m_server.sin_addr.s_addr = INADDR_ANY;
        m_server.sin_port = htons(port);

        int err, one = 1;
        if ((m_fd_server = socket(AF_INET, SOCK_STREAM, 0)) < -1)
            VCML_ERROR("failed to create socket: %s", strerror(m_fd_server));

        if ((err = setsockopt(m_fd_server, SOL_SOCKET, SO_REUSEADDR,
                              (const void*)&one, sizeof(one))))
            VCML_ERROR("setsockopt SO_REUSEADDR failed: %s", strerror(err));

        if ((err = bind(m_fd_server, (struct sockaddr *)&m_server,
                        sizeof(m_server)) < 0))
            VCML_ERROR("binding server socket failed: %s", strerror(err));

        if ((err = ::listen(m_fd_server, 1)))
            VCML_ERROR("listening for connections failed: %s", strerror(err));

        listen_async();
    }

    backend_tcp::~backend_tcp() {
        if (m_fd > -1)
            close(m_fd);
        if (m_fd_server > -1)
            close(m_fd_server);
    }

    void backend_tcp::listen() {
        disconnect();

        socklen_t len = sizeof(m_client);
        if ((m_fd = accept(m_fd_server,
                           (struct sockaddr *)&m_client, &len)) < 0)
            VCML_ERROR("failed to accept connection: %s", strerror(m_fd));

        int err, one = 1;
        if ((err = setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY,
                              (const void*)&one, sizeof(one))))
            VCML_ERROR("setsockopt TCP_NODELAY failed: %s", strerror(err));
    }

    void backend_tcp::listen_async() {
        disconnect();

        aio_notify(m_fd_server, std::bind(&backend_tcp::handle_accept, this,
                   std::placeholders::_1, std::placeholders::_2), AIO_ONCE);
    }

    void backend_tcp::disconnect() {
        if (m_fd > -1) {
            close(m_fd);
            m_fd = -1;
        }
    }

    size_t backend_tcp::peek() {
        return m_fd < 0 ? 0 : backend::peek(m_fd);
    }

    size_t backend_tcp::read(void* buf, size_t len) {
        if (m_fd < 0)
            return 0;

        size_t numread = full_read(m_fd, buf, len);
        if (numread != len)
            listen_async();
        return numread;
    }

    size_t backend_tcp::write(const void* buf, size_t len) {
        if (m_fd < 0)
            return len;

        size_t written = full_write(m_fd, buf, len);
        if (written != len)
            listen_async();
        return written;
    }

    backend* backend_tcp::create(const string& nm) {
        return new backend_tcp(nm.c_str());
    }

}
