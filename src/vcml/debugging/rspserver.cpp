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

#include "vcml/debugging/rspserver.h"

namespace vcml { namespace debugging {

    static inline string opcode(const string& s) {
        size_t pos = s.find(',');
        if (pos == std::string::npos)
            return s;
        return s.substr(0, pos);
    }

    static inline int checksum(const char* str) {
        int result = 0;
        while (str && *str)
            result += static_cast<int>(*str++);
        return result & 0xff;
    }

    static inline int char2int(char c) {
        return ((c >= 'a') && (c <= 'f')) ? c - 'a' + 10 :
               ((c >= 'A') && (c <= 'F')) ? c - 'A' + 10 :
               ((c >= '0') && (c <= '9')) ? c - '0' :
               (c == '\0') ? 0 : -1;
    }

    static inline char int2char(int h) {
        static const char hexchars[] =
            {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
        return hexchars[h & 0xf];
    }

    static void* rsp_thread_func(void* arg) {
        rspserver* server = (rspserver*)arg;
        server->run();
        return NULL;
    }

    void rspserver::send_char(char c) {
        VCML_ERROR_ON(m_fd == -1, "not connected");
        if (send(m_fd, &c, sizeof(c), 0) != sizeof(c))
            VCML_ERROR("error sending data '%c': %s", c, strerror(errno));
    }

    char rspserver::recv_char() {
        VCML_ERROR_ON(m_fd == -1, "not connected");

        char ch = 0;
        if (recv(m_fd, &ch, sizeof(ch), 0) != sizeof(ch))
            VCML_ERROR("error receiving data: %s", strerror(errno));
        return ch;
    }

    rspserver::rspserver(u16 port):
        m_echo(false),
        m_port(port),
        m_fd(-1),
        m_fd_server(-1),
        m_server(),
        m_client(),
        m_running(false),
        m_thread() {
        memset(&m_server, 0, sizeof(m_server));
        memset(&m_client, 0, sizeof(m_client));
        m_server.sin_family = AF_INET;
        m_server.sin_addr.s_addr = INADDR_ANY;
        m_server.sin_port = htons(m_port);
    }

    rspserver::~rspserver() {
        disconnect();
        if (m_fd_server != -1) {
            ::close(m_fd_server);
            m_fd_server = -1;
        }

        pthread_cancel(m_thread);
    }

    void rspserver::send_packet(const char* format, ...) {
        VCML_ERROR_ON(m_fd == -1, "no connection established");

        char packet[VCML_RSP_MAX_PACKET_SIZE] = { 0 };
        packet[0] = '$';

        va_list args;
        va_start(args, format);
        int lim = sizeof(packet) - 4;
        int len = std::vsnprintf(packet + 1, sizeof(packet) - 5, format, args);
        if (len > lim) {
            log_warning("rsp packet too long, truncating %d bytes", len - lim);
            len = lim;
        }
        va_end(args);

        int sum = checksum(packet + 1);
        packet[++len] = '#';
        packet[++len] = int2char((sum >> 4) & 0xf);
        packet[++len] = int2char((sum >> 0) & 0xf);
        packet[++len] = '\0';

        char ack;
        int attempts = 10;

        do {
            if (attempts-- == 0)
                VCML_ERROR("giving up sending packet");
            if (m_echo)
                log_debug("sending packet '%s'", packet);
            if (send(m_fd, packet, len, 0) != len)
                VCML_ERROR("error sending packet: %s", strerror(errno));
            ack = recv_char();
            if (m_echo)
                log_debug("received ack '%c'", ack);
        } while (ack != '+');
    }

    void rspserver::send_packet(const string& s) {
        send_packet(s.c_str());
    }

    string rspserver::recv_packet() {
        unsigned int idx = 0, checksum = 0;
        char packet[VCML_RSP_MAX_PACKET_SIZE];
        while (idx < VCML_RSP_MAX_PACKET_SIZE) {
            char ch = recv_char();
            switch (ch) {
            case '$':
                checksum = 0;
                idx = 0;
                break;

            case '#': {
                packet[idx++] = '\0';

                if (m_echo)
                    log_debug("received packet '%s'", packet);

                unsigned int refsum = 0;
                refsum |= char2int(recv_char()) << 4;
                refsum |= char2int(recv_char()) << 0;

                if (refsum != checksum) {
                    log_debug("checksum mismatch %d != %d", refsum, checksum);
                    send_char('-');
                    checksum = idx = 0;
                    break;
                }

                if (m_echo)
                    log_debug("sending ack '+'");

                send_char('+');
                return packet;
            }

            default:
                checksum = (checksum + ch) & 0xff;
                packet[idx++] = ch;
                break;
            }
        }

        VCML_ERROR("rsp packet length exceeded");
        return "";
    }

    int rspserver::recv_signal(unsigned int timeoutms) {
        if (m_fd == -1)
            return 0;

        fd_set in, out, err;
        struct timeval timeout;
        FD_ZERO(&in);
        FD_SET(m_fd, &in);
        FD_ZERO(&out);
        FD_ZERO(&err);

        timeout.tv_sec  = (timeoutms / 1000u);
        timeout.tv_usec = (timeoutms % 1000u) * 1000u;

        int ret = select(m_fd + 1, &in, NULL, NULL, &timeout);
        if (ret == 0)
            return 0;
        if (ret < 0)
            VCML_ERROR("select call failed: %s", strerror(errno));

        char signal = 0;
        if (recv(m_fd, &signal, 1, MSG_DONTWAIT) != 1)
            VCML_ERROR("recv failed: %s", strerror(errno));
        return (int)signal;
    }

    void rspserver::listen() {
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

        socklen_t len = sizeof(m_client);
        if ((m_fd = accept(m_fd_server,
                           (struct sockaddr *)&m_client, &len)) < 0)
            VCML_ERROR("failed to accept connection: %s", strerror(m_fd));

        if ((err = setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY,
                              (const void*)&one, sizeof(one))))
            VCML_ERROR("setsockopt TCP_NODELAY failed: %s", strerror(err));

        close(m_fd_server);
        m_fd_server = -1;

        handle_connect(inet_ntoa(m_client.sin_addr));
    }

    void rspserver::disconnect() {
        if (m_fd != -1) {
            close(m_fd);
            m_fd = -1;
            handle_disconnect();
        }
    }

    void rspserver::run_async() {
        if (pthread_create(&m_thread, NULL, &rsp_thread_func, this))
            VCML_ERROR("failed to spawn rsp listener thread");

        stringstream ss; ss << "rsp_" << m_port;
        if (pthread_setname_np(m_thread, ss.str().c_str()))
            VCML_ERROR("failed to name rsp listener thread");
    }

    void rspserver::run() {
        m_running = true;
        while (m_running) try {
            disconnect();
            listen();
            while (is_connected()) try {
                string command = recv_packet();
                string response = handle_command(command);
                if (is_connected())
                    send_packet(response);
            } catch (vcml::report& r) {
                log_debug(r.message()); // not an error, e.g. disconnect
                break;
            }
        } catch (vcml::report& r) {
            logger::log(r);
            return;
        }
    }

    void rspserver::stop() {
        VCML_ERROR_ON(!m_running, "server not running");
        m_running = false;
    }

    string rspserver::handle_command(const string& command) {
        try {
            string op = opcode(command);
            if (!stl_contains(m_handlers, op))
                return ""; // empty response means command not supported
            return m_handlers[op](command.c_str());
        } catch (report& rep) {
            logger::log(rep);
            return ERR_INTERNAL;
        } catch (std::exception& ex) {
            log_warn(ex.what());
            return ERR_INTERNAL;
        }
    }

    void rspserver::handle_connect(const char* peer) {
        // to be overloaded
    }

    void rspserver::handle_disconnect() {
        // to be overloaded
    }

    void rspserver::register_handler(const char* cmd, handler h) {
        m_handlers[cmd] = h;
    }

    void rspserver::unregister_handler(const char* cmd) {
        m_handlers.erase(cmd);
    }

    const char* rspserver::ERR_COMMAND  = "E01";
    const char* rspserver::ERR_PARAM    = "E02";
    const char* rspserver::ERR_INTERNAL = "E03";
    const char* rspserver::ERR_UNKNOWN  = "E04";
    const char* rspserver::ERR_PROTOCOL = "E05";

}}
