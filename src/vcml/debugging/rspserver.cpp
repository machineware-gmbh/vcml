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

#include <arpa/inet.h> // for inet_ntoa

#include "vcml/logging/logger.h"
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

    void rspserver::send_char(char c) {
        VCML_ERROR_ON(m_fd == -1, "not connected");
        int res = send(m_fd, &c, sizeof(c), 0);
        if (res < 0)
            VCML_REPORT("error sending data '%c': %s", c, strerror(errno));
        if (res == 0)
            VCML_REPORT("error sending data: disconnected");
    }

    char rspserver::recv_char() {
        VCML_ERROR_ON(m_fd == -1, "not connected");

        char ch = 0;
        int res = recv(m_fd, &ch, sizeof(ch), 0);
        if (res < 0)
            VCML_REPORT("error receiving data: %s", strerror(errno));
        if (res == 0)
            VCML_REPORT("error receiving data: disconnected");
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

        m_running = false;
        if (m_thread.joinable()) {
            // needed to kick thread out of 'accept'
            pthread_cancel(m_thread.native_handle());
            m_thread.join();
        }
    }

    void rspserver::send_packet(const char* format, ...) {
        va_list args;
        va_start(args, format);
        send_packet(vmkstr(format, args));
        va_end(args);
    }

    void rspserver::send_packet(const string& s) {
        VCML_ERROR_ON(m_fd == -1, "no connection established");

        string esc = escape(s, "$#");
        int sum = checksum(esc.c_str());
        stringstream ss;
        ss << "$" << esc << "#"
           << int2char((sum >> 4) & 0xf)
           << int2char((sum >> 0) & 0xf);

        char ack;
        int len = ss.str().length();
        int attempts = 10;

        do {
            if (attempts-- == 0) {
                log_error("giving up sending packet");
                disconnect();
                return;
            }

            if (m_echo)
                log_debug("sending packet '%s'", ss.str().c_str());

            if (send(m_fd, ss.str().c_str(), len, 0) != len) {
                log_error("error sending packet: %s", strerror(errno));
                disconnect();
                return;
            }

            ack = recv_char();
            if (m_echo)
                log_debug("received ack '%c'", ack);
        } while (ack != '+');
    }

    string rspserver::recv_packet() {
        VCML_ERROR_ON(m_fd == -1, "no connection established");
        unsigned int checksum = 0;
        stringstream ss;
        while (true) {
            char ch = recv_char();
            switch (ch) {
            case '$':
                checksum = 0;
                ss.str("");
                break;

            case '#': {
                if (m_echo)
                    log_debug("received packet '%s'", ss.str().c_str());

                unsigned int refsum = 0;
                refsum |= char2int(recv_char()) << 4;
                refsum |= char2int(recv_char()) << 0;

                if (refsum != checksum) {
                    log_debug("checksum mismatch %d != %d", refsum, checksum);
                    send_char('-');
                    checksum = 0;
                    ss.str("");
                    break;
                }

                if (m_echo)
                    log_debug("sending ack '+'");

                send_char('+');
                return ss.str();
            }

            case '\\':
                checksum = (checksum + ch) & 0xff;
                ch = recv_char();
                // no break

            default:
                checksum = (checksum + ch) & 0xff;
                ss << ch;
                break;
            }
        }

        VCML_ERROR("error receiving rsp packet");
        return "";
    }

    int rspserver::recv_signal(time_t timeoutms) {
        if (m_fd == -1)
            return 0;

        if (!fd_peek(m_fd, timeoutms))
            return 0;

        char signal = 0;
        int res = recv(m_fd, &signal, 1, MSG_DONTWAIT);
        if (res < 0)
            VCML_ERROR("recv failed: %s", strerror(errno));
        if (res == 0)
            VCML_ERROR("recv failed: disconnected");
        return (int)signal;
    }

    void rspserver::listen() {
        const int one = 1;

        if (m_fd_server < 0) {
            if ((m_fd_server = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                VCML_ERROR("failed to create socket: %s", strerror(errno));

            if (setsockopt(m_fd_server, SOL_SOCKET, SO_REUSEADDR,
                           (const void*)&one, sizeof(one)))
                VCML_ERROR("setsockopt failed: %s", strerror(errno));

            if (bind(m_fd_server, (sockaddr*)&m_server, sizeof(m_server)))
                VCML_ERROR("binding socket failed: %s", strerror(errno));
        }

        if (::listen(m_fd_server, 1))
            VCML_ERROR("listen for connections failed: %s", strerror(errno));

        socklen_t l = sizeof(m_client);
        if ((m_fd = accept(m_fd_server, (struct sockaddr*)&m_client, &l)) < 0)
            VCML_ERROR("failed to accept connection: %s", strerror(errno));

        if (setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY,
                       (const void*)&one, sizeof(one)) < 0)
            VCML_ERROR("setsockopt TCP_NODELAY failed: %s", strerror(errno));

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
        m_thread = thread(std::bind(&rspserver::run, this));
        stringstream ss; ss << "rsp_" << m_port;
        set_thread_name(m_thread, ss.str());
    }

    void rspserver::run() {
        m_running = true;
        while (m_running) try {
            disconnect();
            listen();
            while (m_running && is_connected()) try {
                string command = recv_packet();
                string response = handle_command(command);
                if (is_connected())
                    send_packet(response);
            } catch (vcml::report& r) {
                log_debug("%s", r.message()); // not an error, e.g. disconnect
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
            log_warn("%s", ex.what());
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
