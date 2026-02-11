/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_VSPSERVER_H
#define VCML_VSPSERVER_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/debugging/rspserver.h"
#include "vcml/debugging/suspender.h"
#include "vcml/debugging/subscriber.h"

namespace vcml {
namespace debugging {

class vspclient;
class vspserver : public rspserver, private suspender
{
private:
    string m_announce;
    sc_time m_duration;
    unordered_map<int, vspclient*> m_clients;

    vspclient& find_client(int client);

    string handle_version(int client, const string& command);
    string handle_status(int client, const string& command);
    string handle_resume(int client, const string& command);
    string handle_step(int client, const string& command);
    string handle_stop(int client, const string& command);
    string handle_quit(int client, const string& command);
    string handle_list(int client, const string& command);
    string handle_exec(int client, const string& command);
    string handle_getq(int client, const string& command);
    string handle_setq(int client, const string& command);
    string handle_geta(int client, const string& command);
    string handle_seta(int client, const string& command);
    string handle_mkbp(int client, const string& command);
    string handle_rmbp(int client, const string& command);
    string handle_mkwp(int client, const string& command);
    string handle_rmwp(int client, const string& command);
    string handle_lreg(int client, const string& command);
    string handle_getr(int client, const string& command);
    string handle_setr(int client, const string& command);
    string handle_vapa(int client, const string& command);
    string handle_vread(int client, const string& command);
    string handle_vwrite(int client, const string& command);

    void disconnect_all();
    void force_quit();
    void notify_step_complete();

public:
    vspserver() = delete;
    vspserver(const string& host, u16 port);
    virtual ~vspserver();

    void start();
    void cleanup();
    void update();
    bool is_running() const { return !is_suspending(); }

    virtual void handle_connect(int client, const string& peer,
                                u16 port) override;
    virtual void handle_disconnect(int client) override;

    static vspserver* instance();
};

} // namespace debugging
} // namespace vcml

#endif
