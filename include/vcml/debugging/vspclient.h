/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_DEBUGGING_VSPCLIENT_H
#define VCML_DEBUGGING_VSPCLIENT_H

#include "vcml/core/systemc.h"
#include "vcml/core/version.h"
#include "vcml/core/component.h"

#include "vcml/debugging/target.h"
#include "vcml/debugging/vspserver.h"

namespace vcml {
namespace debugging {

class vspserver;

class vspclient : public subscriber
{
private:
    vspserver& m_server;
    int m_id;
    u16 m_port;
    string m_peer;
    string m_name;
    sc_time m_until;
    bool m_stop;
    string m_stop_reason;
    mutex m_mtx;

    unordered_map<u64, const breakpoint*> m_breakpoints;
    unordered_map<u64, const watchpoint*> m_watchpoints;

    void resume_simulation(const sc_time& duration);
    void pause_simulation(const string& reason);

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
    constexpr int id() const { return m_id; }
    u16 port() const { return m_port; }
    const char* peer() const { return m_peer.c_str(); }
    const char* name() const { return m_name.c_str(); }
    sc_time until() const { return m_until; }
    bool is_stopped() const { return m_server.is_suspending(); }
    constexpr bool stop_requested() const { return m_stop; }

    vspclient(vspserver& server, int id, const string& peer, u16 port);
    virtual ~vspclient();

    void notify_step_complete();

    string handle_status(const string& command);
    string handle_resume(const string& command);
    string handle_step(const string& command);
    string handle_stop(const string& command);
    string handle_mkbp(const string& command);
    string handle_rmbp(const string& command);
    string handle_mkwp(const string& command);
    string handle_rmwp(const string& command);
};

} // namespace debugging
} // namespace vcml

#endif
