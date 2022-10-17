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

#ifndef VCML_SOCKET_H
#define VCML_SOCKET_H

#include "vcml/core/types.h"

namespace vcml {

class socket
{
private:
    string m_host;
    string m_peer;
    bool m_ipv6;
    atomic<u16> m_port;
    atomic<int> m_socket;
    atomic<int> m_conn;
    thread m_async;

public:
    u16 port() const { return m_port; }
    const char* host() const { return m_host.c_str(); }
    const char* peer() const { return m_peer.c_str(); }

    bool is_ipv4() const { return !m_ipv6; }
    bool is_ipv6() const { return m_ipv6; }

    bool is_listening() const { return m_socket >= 0; }
    bool is_connected() const { return m_conn >= 0; }

    socket();
    socket(u16 port);
    socket(const string& host, u16 port);
    virtual ~socket();

    socket(socket&&) = delete;
    socket(const socket&) = delete;
    socket& operator=(const socket&) = delete;

    void listen(u16 port);
    void unlisten();

    bool accept();
    void accept_async();

    void connect(const string& host, u16 port);
    void disconnect();

    size_t peek(time_t timeoutms = 0);

    void send_char(int c);
    int recv_char();

    void send(const void* data, size_t size);
    void recv(void* data, size_t size);

    void send(const string& str);
    void send(const char* str);

    template <typename T>
    void send(const T& data);
    template <typename T>
    void recv(T& data);
};

inline void socket::send_char(int c) {
    char x = (char)c;
    send(&x, sizeof(x));
}

inline int socket::recv_char() {
    char x = 0;
    recv(&x, sizeof(x));
    return x;
}

inline void socket::send(const string& str) {
    send(str.c_str(), str.length());
}

inline void socket::send(const char* str) {
    send(str, strlen(str));
}

template <typename T>
inline void socket::send(const T& data) {
    send(&data, sizeof(data));
}

template <typename T>
inline void socket::recv(T& data) {
    recv(&data, sizeof(data));
}

} // namespace vcml

#endif
