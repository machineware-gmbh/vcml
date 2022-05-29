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

static int socket_default_address_family() {
    return getenv("VCML_NO_IPv6") ? AF_INET : AF_INET6;
}

#define SET_SOCKOPT(s, lvl, opt, set)                                       \
    do {                                                                    \
        int val = (set);                                                    \
        if (setsockopt(s, lvl, opt, (const void*)&val, sizeof(val)))        \
            VCML_REPORT("setsockopt %s failed: %s", #opt, strerror(errno)); \
    } while (0)

struct socket_addr {
    union {
        sockaddr base;
        sockaddr_in ipv4;
        sockaddr_in6 ipv6;
    };

    socket_addr() { memset(&ipv6, 0, sizeof(ipv6)); }
    socket_addr(const sockaddr* addr);
    socket_addr(int family, u16 port);
    void verify() const;

    bool is_ipv4() const {
        verify();
        return base.sa_family == AF_INET;
    }
    bool is_ipv6() const {
        verify();
        return base.sa_family == AF_INET6;
    }

    string host() const;
    u16 port() const;
    string peer() const;
};

socket_addr::socket_addr(const sockaddr* addr) {
    switch (addr->sa_family) {
    case AF_INET:
        memcpy(&ipv4, addr, sizeof(ipv4));
        break;
    case AF_INET6:
        memcpy(&ipv6, addr, sizeof(ipv6));
        break;
    default:
        VCML_ERROR("accept: unknown protocol family %d", addr->sa_family);
    }
}

socket_addr::socket_addr(int family, u16 port) {
    switch (family) {
    case AF_INET:
        ipv4.sin_family = AF_INET;
        ipv4.sin_addr.s_addr = INADDR_ANY;
        ipv4.sin_port = htons(port);
        break;

    case AF_INET6:
        ipv6.sin6_family = AF_INET6;
        ipv6.sin6_addr = in6addr_any;
        ipv6.sin6_port = htons(port);
        break;

    default:
        VCML_ERROR("accept: unknown protocol family %d", family);
    }
}

void socket_addr::verify() const {
    if (base.sa_family != AF_INET && base.sa_family != AF_INET6)
        VCML_ERROR("accept: unknown protocol family %d", base.sa_family);
}

string socket_addr::host() const {
    switch (base.sa_family) {
    case AF_INET: {
        char str[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &ipv4.sin_addr, str, sizeof(str));
        return str;
    }

    case AF_INET6: {
        char str[INET6_ADDRSTRLEN] = {};
        inet_ntop(AF_INET6, &ipv6.sin6_addr, str, sizeof(str));
        return str;
    }

    default:
        return "unknown";
    }
}

u16 socket_addr::port() const {
    switch (base.sa_family) {
    case AF_INET:
        return ntohs(ipv4.sin_port);
    case AF_INET6:
        return ntohs(ipv6.sin6_port);
    default:
        return 0;
    }
}

string socket_addr::peer() const {
    return mkstr("%s:%hu", host().c_str(), port());
}

socket::socket():
    m_host(),
    m_peer(),
    m_ipv6(),
    m_port(0),
    m_socket(-1),
    m_conn(-1),
    m_async() {
}

socket::socket(u16 port):
    m_host(),
    m_peer(),
    m_ipv6(),
    m_port(0),
    m_socket(-1),
    m_conn(-1),
    m_async() {
    listen(port);
}

socket::socket(const string& host, u16 port):
    m_host(),
    m_peer(),
    m_ipv6(),
    m_port(0),
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

    int family = socket_default_address_family();

    m_socket = ::socket(family, SOCK_STREAM, 0);
    if (m_socket < 0)
        VCML_REPORT("failed to create socket: %s", strerror(errno));

    SET_SOCKOPT(m_socket, SOL_SOCKET, SO_REUSEADDR, 1);
    if (family == AF_INET6)
        SET_SOCKOPT(m_socket, IPPROTO_IPV6, IPV6_V6ONLY, 0);

    if (port > 0) {
        socket_addr addr(family, port);
        if (::bind(m_socket, &addr.base, sizeof(addr))) {
            VCML_REPORT("binding socket to port %hu failed: %s", port,
                        strerror(errno));
        }
    }

    if (::listen(m_socket, 1))
        VCML_REPORT("listen for connections failed: %s", strerror(errno));

    socket_addr addr;
    socklen_t len = sizeof(addr);
    if (getsockname(m_socket, &addr.base, &len) < 0)
        VCML_ERROR("getsockname failed: %s", strerror(errno));

    m_ipv6 = family == AF_INET6;
    m_host = m_ipv6 ? "::1" : "127.0.0.1";
    m_port = addr.port();
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

bool socket::accept() {
    if (is_connected())
        disconnect();

    socket_addr addr;
    socklen_t len = sizeof(addr);

    m_conn = ::accept(m_socket, &addr.base, &len);
    if (m_conn < 0 && m_socket < 0)
        return false; // shutdown while waiting for connections

    if (m_conn < 0)
        VCML_ERROR("failed to accept connection: %s", strerror(errno));

    SET_SOCKOPT(m_conn, IPPROTO_TCP, TCP_NODELAY, 1);

    m_ipv6 = addr.is_ipv6();
    m_peer = mkstr("%s:%hu", addr.host().c_str(), addr.port());
    return true;
}

void socket::accept_async() {
    if (!is_listening())
        VCML_ERROR("socket not listening");
    if (m_async.joinable())
        VCML_ERROR("socket already accepting connections");

    if (is_connected())
        disconnect();

    m_async = thread(std::bind(&socket::accept, this));
    set_thread_name(m_async, mkstr("socket_%hu", port()));
}

void socket::connect(const string& host, u16 port) {
    if (is_connected())
        disconnect();

    string pstr = to_string(port);

    addrinfo hint = {};
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = IPPROTO_TCP;

    addrinfo* ai;
    int err = getaddrinfo(host.c_str(), pstr.c_str(), &hint, &ai);
    VCML_REPORT_ON(err, "getaddrinfo failed: %s", gai_strerror(err));
    if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
        VCML_ERROR("getaddrinfo: protocol family %d", ai->ai_family);

    for (; ai != nullptr; ai = ai->ai_next) {
        m_conn = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (m_conn < 0)
            VCML_REPORT("failed to create socket: %s", strerror(errno));

        if (::connect(m_conn, ai->ai_addr, ai->ai_addrlen) < 0) {
            close(m_conn);
            continue;
        }

        m_ipv6 = ai->ai_family == AF_INET6;
        m_peer = socket_addr(ai->ai_addr).peer();
        break;
    }

    freeaddrinfo(ai);
    VCML_REPORT_ON(m_peer.empty(), "connect failed: %s", strerror(errno));
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

} // namespace vcml
