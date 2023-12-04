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

rspserver::rspserver(u16 port):
    m_sock(port),
    m_port(m_sock.port()),
    m_name(mkstr("rsp_%hu", m_port)),
    m_echo(false),
    m_running(false),
    m_mutex(),
    m_thread(),
    m_handlers(),
    log(m_name) {
}

rspserver::~rspserver() {
    if (m_thread.joinable())
        VCML_ERROR("rspserver still running");
}

void rspserver::send_packet(const char* format, ...) {
    va_list args;
    va_start(args, format);
    send_packet(vmkstr(format, args));
    va_end(args);
}

void rspserver::send_packet(const string& s) {
    VCML_ERROR_ON(!is_connected(), "no connection established");
    string esc = rsp_escape(s);

    stringstream ss;
    u8 sum = checksum(esc.c_str());
    ss << "$" << esc << "#" << to_hex_ascii(sum >> 4) << to_hex_ascii(sum);

    char ack;
    size_t attempts = 10;
    lock_guard<mutex> lock(m_mutex);

    do {
        if (attempts-- == 0) {
            log_error("giving up sending packet");
            disconnect();
            return;
        }

        if (m_echo)
            log_debug("sending packet '%s'", ss.str().c_str());

        m_sock.send(ss.str());

        do {
            ack = m_sock.recv_char();
        } while (ack != '+' && ack != '-');

        if (m_echo)
            log_debug("received ack '%c'", ack);
    } while (ack != '+');
}

string rspserver::recv_packet() {
    lock_guard<mutex> lock(m_mutex);
    VCML_ERROR_ON(!is_connected(), "no connection established");

    u8 checksum = 0;
    stringstream ss;

    while (true) {
        char ch = m_sock.recv_char();
        switch (ch) {
        case '$':
            checksum = 0;
            ss.str("");
            break;

        case '#': {
            if (m_echo)
                log_debug("received packet '%s'", ss.str().c_str());

            u8 refsum = 0;
            refsum |= from_hex_ascii(m_sock.recv_char()) << 4;
            refsum |= from_hex_ascii(m_sock.recv_char()) << 0;

            if (refsum != checksum) {
                log_warn("checksum mismatch %02x != %02x", refsum, checksum);
                m_sock.send_char('-');
                checksum = 0;
                ss.str("");
                break;
            }

            if (m_echo)
                log_debug("sending ack '+'");

            m_sock.send_char('+');
            return ss.str();
        }

        case '}':
            checksum += ch;
            ch = m_sock.recv_char();
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

    VCML_ERROR("error receiving rsp packet");
    return "";
}

int rspserver::recv_signal(time_t timeoutms) {
    lock_guard<mutex> lock(m_mutex);

    try {
        if (!is_connected())
            return 0;
        if (!m_sock.peek(timeoutms))
            return 0;
        return m_sock.recv_char();
    } catch (...) {
        return -1;
    }
}

void rspserver::listen() {
    m_sock.listen(m_port);
    if (m_sock.accept()) {
        m_sock.unlisten();
        if (m_running)
            handle_connect(m_sock.peer());
    }
}

void rspserver::disconnect() {
    if (m_sock.is_connected()) {
        m_sock.disconnect();
        if (m_running)
            handle_disconnect();
    }
}

void rspserver::run_async() {
    m_running = true;
    m_thread = thread(&rspserver::run, this);
}

void rspserver::run() {
    mwr::set_thread_name(m_name);
    while (m_running) {
        try {
            disconnect();
            listen();
            while (m_running && is_connected())
                try {
                    string command = recv_packet();
                    string response = handle_command(command);
                    if (is_connected())
                        send_packet(response);
                } catch (vcml::report& r) {
                    log_debug("%s", r.message());
                    break; // not an error, e.g. disconnect
                }
        } catch (vcml::report& r) {
            log.error(r);
        }
    }
}

void rspserver::stop() {
    m_running = false;
}

void rspserver::shutdown() {
    stop();

    if (m_sock.is_listening())
        m_sock.unlisten();
    if (m_sock.is_connected())
        m_sock.disconnect();

    if (m_thread.joinable())
        m_thread.join();
}

string rspserver::handle_command(const string& command) {
    try {
        for (const auto& handler : m_handlers)
            if (starts_with(command, handler.first))
                return handler.second(command);
        return ""; // empty response means command not supported
    } catch (report& rep) {
        log.error(rep);
        return ERR_INTERNAL;
    } catch (std::exception& ex) {
        log.error(ex);
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
    for (const auto& other : m_handlers) {
        if (starts_with(other.first, cmd) || starts_with(cmd, other.first))
            VCML_ERROR("overlapping handlers %s %s", cmd, other.first.c_str());
    }

    m_handlers[cmd] = std::move(h);
}

void rspserver::unregister_handler(const char* cmd) {
    m_handlers.erase(cmd);
}

const char* const rspserver::ERR_COMMAND = "E01";
const char* const rspserver::ERR_PARAM = "E02";
const char* const rspserver::ERR_INTERNAL = "E03";
const char* const rspserver::ERR_UNKNOWN = "E04";
const char* const rspserver::ERR_PROTOCOL = "E05";

} // namespace debugging
} // namespace vcml
