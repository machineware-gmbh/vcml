/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_RSPSERVER_H
#define VCML_RSPSERVER_H

#include "vcml/core/types.h"
#include "vcml/logging/logger.h"

namespace vcml {
namespace debugging {

class rspserver
{
public:
    typedef function<string(const string&)> handler;

private:
    mwr::socket m_sock;

    u16 m_port;
    string m_name;

    atomic<bool> m_echo;
    atomic<bool> m_running;

    mutex m_mutex;
    thread m_thread;

    std::map<string, handler> m_handlers;

    // disabled
    rspserver();
    rspserver(const rspserver&);

public:
    logger log;

    u16 port() const { return m_port; }
    const char* name() const { return m_name.c_str(); }

    bool is_connected() const { return m_sock.is_connected(); }
    bool is_listening() const { return m_sock.is_listening(); }

    void echo(bool e = true) { m_echo = e; }

    rspserver(u16 port);
    virtual ~rspserver();

    void send_packet(const string& s);
    void send_packet(const char* format, ...);
    string recv_packet();
    int recv_signal(time_t timeoutms = ~0ull);

    void listen();
    void disconnect();

    void run_async();
    void run();
    void stop();
    void shutdown();

    virtual string handle_command(const string& command);
    virtual void handle_connect(const char* peer);
    virtual void handle_disconnect();

    template <typename T>
    void register_handler(const char* cmd, string (T::*func)(const string&));

    void register_handler(const char* command, handler handler);
    void unregister_handler(const char* command);

    static const char* const ERR_COMMAND;  // malformed command
    static const char* const ERR_PARAM;    // parameter has invalid value
    static const char* const ERR_INTERNAL; // internal error
    static const char* const ERR_UNKNOWN;  // unknown error
    static const char* const ERR_PROTOCOL; // protocol error
};

template <typename HOST>
void rspserver::register_handler(const char* command,
                                 string (HOST::*handler)(const string&)) {
    HOST* host = dynamic_cast<HOST*>(this);
    VCML_ERROR_ON(!host, "command host not found");
    register_handler(command, [host, handler](const string& args) -> string {
        return (host->*handler)(args);
    });
}

} // namespace debugging
} // namespace vcml

#endif
