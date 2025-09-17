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

class vspserver : public rspserver, private suspender, private subscriber
{
private:
    string m_announce;
    string m_stop_reason;
    sc_time m_duration;

    unordered_map<u64, const breakpoint*> m_breakpoints;
    unordered_map<u64, const watchpoint*> m_watchpoints;

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

    bool is_running() const { return !is_suspending(); }

    void resume_simulation(const sc_time& duration);
    void pause_simulation(const string& reason);
    void force_quit();

    virtual void notify_step_complete(target& tgt, const sc_time& t) override;

    virtual void notify_breakpoint_hit(const breakpoint& bp,
                                       const sc_time& t) override;

    virtual void notify_watchpoint_read(const watchpoint& wp,
                                        const range& addr,
                                        const sc_time& t) override;

    virtual void notify_watchpoint_write(const watchpoint& wp,
                                         const range& addr, const void* newval,
                                         const sc_time& t) override;

public:
    vspserver() = delete;
    explicit vspserver(const string& host, u16 port);
    virtual ~vspserver();

    void start();
    void cleanup();

    virtual void handle_connect(int client, const string& peer) override;
    virtual void handle_disconnect(int client) override;

    static vspserver* instance();
};

} // namespace debugging
} // namespace vcml

#endif
