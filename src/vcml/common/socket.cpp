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
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "vcml/common/socket.h"

namespace vcml {

    socket::socket():
        m_port(0),
        m_host(),
        m_peer(),
        m_ipv6(),
        m_socket(-1),
        m_conn(-1),
        m_async() {
    }

    socket::socket(u16 port):
        m_port(0),
        m_host(),
        m_peer(),
        m_ipv6(),
        m_socket(-1),
        m_conn(-1),
        m_async() {
        listen(port);
    }

    socket::socket(const string& host, u16 port):
        m_port(0),
        m_host(),
        m_peer(),
        m_ipv6(),
        m_socket(-1),
        m_conn(-1),
        m_async() {
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

        m_socket = ::socket(AF_INET6, SOCK_STREAM, 0);
        if (m_socket < 0)
            VCML_REPORT("failed to create socket: %s", strerror(errno));

        const int yes = 1, no = 0;
        if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR,
                       (const void*)&yes, sizeof(yes))) {
            VCML_REPORT("setsockopt SO_REUSEADDR failed: %s", strerror(errno));
        }

        if (setsockopt(m_socket, IPPROTO_IPV6, IPV6_V6ONLY,
                       (const void*)&no, sizeof(no))) {
            VCML_REPORT("setsockopt IPV6_V6ONLY failed: %s", strerror(errno));
        }


        if (port > 0) {
            sockaddr_in6 addr = {};
            addr.sin6_family = AF_INET6;
            addr.sin6_addr = in6addr_any;
            addr.sin6_port = htons(port);
            if (::bind(m_socket, (sockaddr*)&addr, sizeof(addr))) {
                VCML_REPORT("binding socket to port %hu failed: %s", port,
                            strerror(errno));
            }
        }

        if (::listen(m_socket, 1))
            VCML_REPORT("listen for connections failed: %s", strerror(errno));

        sockaddr_in6 addr = {};
        socklen_t len = sizeof(addr);
        if (getsockname(m_socket, (struct sockaddr *)&addr, &len) < 0)
            VCML_ERROR("getsockname failed: %s", strerror(errno));

        char str[INET6_ADDRSTRLEN] = {};
        inet_ntop(addr.sin6_family, &addr.sin6_addr, str, INET6_ADDRSTRLEN);
        m_host = mkstr("%s", str);
        m_port = ntohs(addr.sin6_port);
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

        sockaddr_in6 addr = {};
        socklen_t len = sizeof(addr);
        m_conn = ::accept(m_socket, (struct sockaddr*)&addr, &len);

        if (m_conn < 0 && m_socket < 0)
            return; // shutdown while waiting for connections

        if (m_conn < 0)
            VCML_ERROR("failed to accept connection: %s", strerror(errno));

        if (addr.sin6_family != AF_INET && addr.sin6_family != AF_INET6)
            VCML_ERROR("accept: protocol family %d", addr.sin6_family);
        m_ipv6 = addr.sin6_family == AF_INET6;

        const int one = 1;
        if (setsockopt(m_conn, IPPROTO_TCP, TCP_NODELAY,
                       (const void*)&one, sizeof(one)) < 0) {
            VCML_ERROR("setsockopt TCP_NODELAY failed: %s", strerror(errno));
        }

        char str[INET6_ADDRSTRLEN] = {};
        inet_ntop(addr.sin6_family, &addr.sin6_addr, str, INET6_ADDRSTRLEN);
        m_peer = mkstr("%s:%hu", str, ntohs(addr.sin6_port));
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

        string pstr = to_string(port);

        struct addrinfo hint = {};
        hint.ai_family = AF_UNSPEC;
        hint.ai_socktype = SOCK_STREAM;

        struct addrinfo* res;
        int err = getaddrinfo(host.c_str(), pstr.c_str(), &hint, &res);
        VCML_REPORT_ON(err, "getaddrinfo failed: %s", gai_strerror(err));
        if (res->ai_family != AF_INET && res->ai_family != AF_INET6)
            VCML_ERROR("getaddrinfo: protocol family %d", res->ai_family);

        m_ipv6 = res->ai_family == AF_INET6;
        m_conn = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (m_conn < 0)
            VCML_REPORT("failed to create socket: %s", strerror(errno));

        if (::connect(m_conn, res->ai_addr, res->ai_addrlen) < 0)
            VCML_REPORT("connect failed: %s", strerror(errno));

        m_peer = mkstr("%s:%hu", host.c_str(), port);
        freeaddrinfo(res);
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
        if (m_async.joinable())
            m_async.join();

        if (!is_connected())
            VCML_REPORT("error receiving data: not connected");

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
        if (m_async.joinable())
            m_async.join();

        if (!is_connected())
            VCML_REPORT("error receiving data: not connected");

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
