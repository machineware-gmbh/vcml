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
    typedef function<string(int, const string&)> handler;

private:
    mwr::server_socket m_sock;

    u16 m_port;
    string m_name;

    atomic<bool> m_echo;
    atomic<bool> m_running;

    mutex m_mutex;
    thread m_thread;
    condition_variable_any m_cv;

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

    rspserver(const string& host, u16 port, size_t max_clients);
    virtual ~rspserver();

    void send_packet(int client, const string& s);
    void send_packet(int client, const char* format, ...);
    string recv_packet(int client);
    int recv_signal(int client, time_t timeoutms = ~0ull);

    void run_async();
    void run();
    void stop();
    void shutdown();
    void disconnect(int client);

    virtual string handle_command(int client, const string& command);
    virtual void handle_connect(int client, const string& peer);
    virtual void handle_disconnect(int client);

    template <typename T>
    void register_handler(const char* cmd,
                          string (T::*func)(int, const string&));
    void register_handler(const char* command, handler handler);
    void unregister_handler(const char* command);
};

template <typename HOST>
void rspserver::register_handler(const char* command,
                                 string (HOST::*fn)(int, const string&)) {
    HOST* host = dynamic_cast<HOST*>(this);
    VCML_ERROR_ON(!host, "command host not found");
    register_handler(command, [host, fn](int c, const string& a) -> string {
        return (host->*fn)(c, a);
    });
}

inline string rsp_error(int eno) {
    return mkstr("E%02x", eno);
}

} // namespace debugging
} // namespace vcml

#endif
