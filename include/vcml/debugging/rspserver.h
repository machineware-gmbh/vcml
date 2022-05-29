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

#ifndef VCML_RSPSERVER_H
#define VCML_RSPSERVER_H

#include "vcml/common/types.h"
#include "vcml/common/strings.h"
#include "vcml/common/report.h"
#include "vcml/common/socket.h"

#include "vcml/logging/logger.h"

namespace vcml {
namespace debugging {

class rspserver
{
public:
    typedef function<string(const char*)> handler;

private:
    socket m_sock;
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
    void register_handler(const char* cmd, string (T::*handler)(const char*));

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
                                 string (HOST::*handler)(const char*)) {
    HOST* host = dynamic_cast<HOST*>(this);
    VCML_ERROR_ON(!host, "command host not found");
    register_handler(command, [host, handler](const char* args) -> string {
        return (host->*handler)(args);
    });
}

} // namespace debugging
} // namespace vcml

#endif
