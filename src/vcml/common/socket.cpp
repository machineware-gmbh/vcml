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

#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "vcml/common/socket.h"

namespace vcml {

    socket::socket():
        m_port(0),
        m_host(),
        m_socket(-1),
        m_conn(-1) {
    }

    socket::socket(u16 port):
        m_port(0),
        m_host(),
        m_socket(-1),
        m_conn(-1) {
        listen(port);
    }

    socket::socket(const string& host, u16 port):
        m_port(0),
        m_host(),
        m_socket(-1),
        m_conn(-1) {
        connect(host, port);
    }

    socket::~socket() {
        if (is_connected())
            disconnect();
        if (is_listening())
            unlisten();
    }

    void socket::listen(u16 port) {
        if (is_listening() && (port == 0 || port == m_port))
            return;

        unlisten();

        m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (m_socket < 0)
            VCML_ERROR("failed to create socket: %s", strerror(errno));

        const int one = 1;
        if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR,
                       (const void*)&one, sizeof(one))) {
            VCML_ERROR("setsockopt failed: %s", strerror(errno));
        }

        if (port > 0) {
            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(port);
            if (::bind(m_socket, (sockaddr*)&addr, sizeof(addr))) {
                VCML_ERROR("binding socket to port %hu failed: %s", port,
                           strerror(errno));
            }
        }

        if (::listen(m_socket, 1))
            VCML_ERROR("listen for connections failed: %s", strerror(errno));

        sockaddr_in addr = {};
        socklen_t len = sizeof(addr);
        if (getsockname(m_socket, (struct sockaddr *)&addr, &len) < 0)
            VCML_ERROR("getsockname failed: %s", strerror(errno));

        m_host = mkstr("%s", inet_ntoa(addr.sin_addr));
        m_port = ntohs(addr.sin_port);
        VCML_ERROR_ON(m_port == 0, "port cannot be zero");
    }

    void socket::unlisten() {
        if (!is_listening())
            return;

        int fd = m_socket;
        m_socket = -1;
        ::shutdown(fd, SHUT_RDWR);

        if (m_async.joinable())
            m_async.join();

        m_host.clear();
        m_port = 0;
    }

    void socket::accept() {
        if (is_connected())
            disconnect();

        sockaddr_in addr = {};
        socklen_t len = sizeof(addr);
        m_conn = ::accept(m_socket, (struct sockaddr*)&addr, &len);

        if (m_conn < 0 && m_socket < 0)
            return; // shutdown while waiting for connections

        if (m_conn < 0)
            VCML_ERROR("failed to accept connection: %s", strerror(errno));

        const int one = 1;
        if (setsockopt(m_conn, IPPROTO_TCP, TCP_NODELAY,
                       (const void*)&one, sizeof(one)) < 0) {
            VCML_ERROR("setsockopt TCP_NODELAY failed: %s", strerror(errno));
        }

        m_peer = mkstr("%s", inet_ntoa(addr.sin_addr));
    }

    void socket::accept_async() {
        if (!is_listening())
            VCML_ERROR("socket not listening");
        if (m_async.joinable())
            VCML_ERROR("socket already accepting connections");

        if (is_connected())
            disconnect();

        m_async = thread(std::bind(&socket::accept, this));
        set_thread_name(m_async, mkstr("socket_%hu", m_port));
    }

    void socket::connect(const string& host, u16 port) {
        if (is_connected())
            disconnect();

        m_conn = ::socket(AF_INET, SOCK_STREAM, 0);
        if (m_conn < 0)
            VCML_ERROR("failed to create socket: %s", strerror(errno));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        int err = inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
        if (err == 0)
            VCML_REPORT("host address invalid: %s", host.c_str());
        if (err < 0)
            VCML_REPORT("host lookup failed: %s", strerror(errno));

        if (::connect(m_conn, (struct sockaddr*)&addr, sizeof(addr)) < 0)
            VCML_REPORT("connect failed: %s", strerror(errno));
    }

    void socket::disconnect() {
        if (!is_connected())
            return;

        int fd = m_conn;
        m_conn = -1;
        ::shutdown(fd, SHUT_RDWR);

        m_peer.clear();
    }

    size_t socket::peek(time_t timeoutms) {
        if (!is_connected())
            return 0;

        if (m_async.joinable())
            m_async.join();

        if (!fd_peek(m_conn, timeoutms))
            return 0;

        char buf[32];
        int err = ::recv(m_conn, buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);
        if (err <= 0)
            disconnect();
        if (err == 0)
            VCML_REPORT("error receiving data: disconnected");
        if (err < 0)
            VCML_REPORT("error receiving data: %s", strerror(errno));

        return err;
    }

    void socket::send(const void* data, size_t size) {
        VCML_ERROR_ON(!is_connected(), "not connected");

        if (m_async.joinable())
            m_async.join();

        const u8* ptr = (const u8*)data;
        size_t n = 0;

        while (n < size) {
            int r = ::send(m_conn, ptr + n, size - n, 0);
            if (r <= 0)
                disconnect();
            if (r == 0)
                VCML_REPORT("error sending data: disconnected");
            if (r < 0)
                VCML_REPORT("error sending data: %s", strerror(errno));

            n += r;
        }
    }

    void socket::recv(void* data, size_t size) {
        VCML_ERROR_ON(!is_connected(), "not connected");

        if (m_async.joinable())
            m_async.join();

        u8* ptr = (u8*)data;
        size_t n = 0;

        while (n < size) {
            int r = ::recv(m_conn, ptr + n, size - n, 0);
            if (r <= 0)
                disconnect();
            if (r == 0)
                VCML_REPORT("error receiving data: disconnected");
            if (r < 0)
                VCML_REPORT("error receiving data: %s", strerror(errno));

            n += r;
        }
    }

}
