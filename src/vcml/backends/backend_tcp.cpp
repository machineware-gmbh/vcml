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

#include "vcml/common/aio.h"
#include "vcml/backends/backend_tcp.h"

namespace vcml {

    static void set_nodelay(int fd) {
        const int one = 1;
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const void*)&one,
            sizeof(one)) < 0) {
            VCML_REPORT("setsockopt TCP_NODELAY failed: %s", strerror(errno));
        }
    }

    static void set_blocking(int fd, bool blocking) {
        int flags = fcntl(fd, F_GETFL, 0);
        VCML_REPORT_ON(flags < 0, "fcntl failed: %s", strerror(errno));
        flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
        int err = fcntl(fd, F_SETFL, flags);
        VCML_REPORT_ON(err < 0, "fcntl failed: %s", strerror(errno));
    }

    void backend_tcp::handle_accept(int fd, int events) {
        if (fd != m_fd_server)
            return;

        accept();
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

        const int one = 1;
        if ((m_fd_server = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            VCML_REPORT("failed to create socket: %s", strerror(errno));

        if (setsockopt(m_fd_server, SOL_SOCKET, SO_REUSEADDR,
                       (const void*)&one, sizeof(one)))
            VCML_REPORT("setsockopt SO_REUSEADDR failed: %s", strerror(errno));

        if (bind(m_fd_server, (struct sockaddr*)&m_server, sizeof(m_server)))
            VCML_REPORT("binding server socket failed: %s", strerror(errno));

        if (::listen(m_fd_server, 1))
            VCML_REPORT("listen for connections failed: %s", strerror(errno));

        accept_async();
    }

    backend_tcp::~backend_tcp() {
        if (m_fd > -1)
            close(m_fd);
        if (m_fd_server > -1)
            close(m_fd_server);
    }

    void backend_tcp::accept() {
        log_debug("listening for incoming connections");

        socklen_t len = sizeof(m_client);
        m_fd = ::accept(m_fd_server, (struct sockaddr *)&m_client, &len);
        if (m_fd < 0 && errno == EAGAIN)
            return;

        VCML_REPORT_ON(m_fd < 0, "accept failed: %s", strerror(errno));
        set_nodelay(m_fd);
        log_debug("connected");
    }

    void backend_tcp::accept_async() {
        VCML_REPORT_ON(m_fd >= 0, "previous connection not closed");
        set_blocking(m_fd_server, false);
        accept();
        set_blocking(m_fd_server, true);

        if (m_fd < 0) {
            log_debug("accepting asynchronously");
            aio_notify(m_fd_server, std::bind(&backend_tcp::handle_accept, this,
                       std::placeholders::_1, std::placeholders::_2), AIO_ONCE);
        }
    }

    void backend_tcp::disconnect() {
        if (m_fd > -1) {
            log_debug("disconnected");
            close(m_fd);
            m_fd = -1;
        }
    }

    size_t backend_tcp::peek() {
        if (m_fd < 0)
            return 0;

        char buf[32];
        int nbytes = recv(m_fd, buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);
        if (nbytes > 0)
            return nbytes;
        if (nbytes == 0) {
            disconnect();
            accept_async();
            return 0;
        }

        VCML_REPORT_ON(errno != EAGAIN, "unexpected recv result");
        return 0;
    }

    size_t backend_tcp::read(void* buf, size_t len) {
        if (m_fd < 0)
            return 0;

        size_t numread = fd_read(m_fd, buf, len);
        if (numread != len) {
            disconnect();
            accept_async();
        }

        return numread;
    }

    size_t backend_tcp::write(const void* buf, size_t len) {
        if (m_fd < 0)
            return len;

        size_t written = fd_write(m_fd, buf, len);
        if (written != len) {
            disconnect();
            accept_async();
        }

        return written;
    }

    backend* backend_tcp::create(const string& nm) {
        return new backend_tcp(nm.c_str());
    }

}
