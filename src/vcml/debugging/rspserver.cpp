/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/logging/logger.h"
#include "vcml/debugging/rspserver.h"

namespace vcml {
namespace debugging {

static bool needs_escape(char c) {
    return c == '$' || c == '#' || c == '}' || c == '*';
}

static string rsp_escape(const string& s) {
    stringstream ss;
    for (char c : s) {
        if (needs_escape(c)) {
            c ^= 0x20;
            ss << '}' << c;
        } else {
            ss << c;
        }
    }

    return ss.str();
}

static u8 checksum(const char* str) {
    u8 result = 0;
    while (str && *str)
        result += static_cast<u8>(*str++);
    return result;
}

rspserver::rspserver(const string& host, u16 port, size_t max_clients):
    m_sock(max_clients, port, host),
    m_port(m_sock.port()),
    m_name(mkstr("rsp_%hu", m_port)),
    m_echo(false),
    m_running(false),
    m_mutex(),
    m_thread(),
    m_handlers(),
    log(m_name) {
    m_sock.on_connect([&](int client, const string& peer, u16 port) -> bool {
        if (m_running)
            handle_connect(client, peer, port);
        return true;
    });

    m_sock.on_disconnect([&](int client) {
        if (m_running)
            handle_disconnect(client);
    });

    m_sock.set_tcp_nodelay();
}

rspserver::~rspserver() {
    if (m_thread.joinable())
        VCML_ERROR("rspserver still running");
}

void rspserver::send_packet(int client, const char* format, ...) {
    va_list args;
    va_start(args, format);
    send_packet(client, vmkstr(format, args));
    va_end(args);
}

void rspserver::send_packet(int client, const string& s) {
    VCML_REPORT_ON(!is_connected(), "no connection established");
    string esc = rsp_escape(s);

    stringstream ss;
    u8 sum = checksum(esc.c_str());
    ss << "$" << esc << "#" << to_hex_ascii(sum >> 4) << to_hex_ascii(sum);

    char ack;
    size_t attempts = 10;
    lock_guard<mutex> lock(m_mutex);

    do {
        if (attempts-- == 0) {
            m_sock.disconnect(client);
            VCML_REPORT("client%d: giving up sending packet", client);
        }

        if (m_echo)
            log_debug("sending packet '%s'", ss.str().c_str());

        m_sock.send(client, ss.str());

        do {
            ack = m_sock.recv_char(client);
        } while (ack != '+' && ack != '-');

        if (m_echo)
            log_debug("received ack '%c'", ack);
    } while (ack != '+');
}

string rspserver::recv_packet(int client) {
    lock_guard<mutex> lock(m_mutex);
    VCML_REPORT_ON(!is_connected(), "no connection established");

    u8 checksum = 0;
    stringstream ss;

    while (true) {
        char ch = m_sock.recv_char(client);
        switch (ch) {
        case '$':
            checksum = 0;
            ss.str("");
            break;

        case '#': {
            if (m_echo)
                log_debug("received packet '%s'", ss.str().c_str());

            u8 refsum = 0;
            refsum |= from_hex_ascii(m_sock.recv_char(client)) << 4;
            refsum |= from_hex_ascii(m_sock.recv_char(client)) << 0;

            if (refsum > 0 && refsum != checksum) {
                log_warn("checksum mismatch %02x != %02x", refsum, checksum);
                m_sock.send_char(client, '-');
                checksum = 0;
                ss.str("");
                break;
            }

            if (m_echo)
                log_debug("sending ack '+'");

            m_sock.send_char(client, '+');
            return ss.str();
        }

        case '}':
            checksum += ch;
            ch = m_sock.recv_char(client);
            checksum += ch;
            ch ^= 0x20;
            if (!needs_escape(ch))
                log_warn("escaped invalid char 0x%02hhx", ch);
            ss << ch;
            break;

        default:
            checksum += ch;
            ss << ch;
            break;
        }
    }

    VCML_REPORT("error receiving rsp packet");
    return "";
}

int rspserver::recv_signal(int client, time_t timeoutms) {
    lock_guard<mutex> lock(m_mutex);

    try {
        if (!m_sock.peek(client, timeoutms))
            return 0;
        return m_sock.recv_char(client);
    } catch (...) {
        return -1;
    }
}

void rspserver::run_async() {
    m_mutex.lock();
    m_thread = thread([&]() {
        mwr::set_thread_name(m_name);
        run();
    });
    m_cv.wait(m_mutex, [&]() -> bool { return m_running; });
    m_mutex.unlock();
}

void rspserver::run() {
    m_mutex.lock();
    m_running = true;
    m_cv.notify_all();
    m_mutex.unlock();

    while (m_running) {
        try {
            int client = m_sock.poll(100);
            if (client >= 0)
                process(client);
        } catch (std::exception& ex) {
            log.warn(ex);
        }
    }
}

void rspserver::stop() {
    m_running = false;
}

void rspserver::shutdown() {
    stop();

    m_sock.disconnect_all();

    if (m_thread.joinable())
        m_thread.join();
}

void rspserver::disconnect(int client) {
    if (m_sock.is_connected(client))
        m_sock.disconnect(client);
}

void rspserver::process(int client) {
    try {
        string command = recv_packet(client);
        string response = handle_command(client, command);
        send_packet(client, response);
    } catch (std::exception& ex) {
        log_debug("client%d: %s", client, ex.what());
    }
}

string rspserver::handle_command(int client, const string& command) {
    try {
        for (const auto& handler : m_handlers)
            if (starts_with(command, handler.first))
                return handler.second(client, command);
        return ""; // empty response means command not supported
    } catch (report& rep) {
        log.error(rep);
        return rsp_error(EINVAL);
    } catch (std::exception& ex) {
        log.error(ex);
        return rsp_error(EINVAL);
    }
}

void rspserver::handle_connect(int client, const string& peer, u16 port) {
    // to be overloaded
}

void rspserver::handle_disconnect(int client) {
    // to be overloaded
}

void rspserver::register_handler(const char* cmd, handler h) {
    for (const auto& other : m_handlers) {
        if (starts_with(other.first, cmd) || starts_with(cmd, other.first))
            VCML_ERROR("overlapping handlers %s %s", cmd, other.first.c_str());
    }

    m_handlers[cmd] = std::move(h);
}

void rspserver::unregister_handler(const char* cmd) {
    m_handlers.erase(cmd);
}

} // namespace debugging
} // namespace vcml
