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

    string handle_version(const string& command);
    string handle_status(const string& command);
    string handle_resume(const string& command);
    string handle_step(const string& command);
    string handle_stop(const string& command);
    string handle_quit(const string& command);
    string handle_list(const string& command);
    string handle_exec(const string& command);
    string handle_getq(const string& command);
    string handle_setq(const string& command);
    string handle_geta(const string& command);
    string handle_seta(const string& command);
    string handle_mkbp(const string& command);
    string handle_rmbp(const string& command);

    bool is_running() const { return !is_suspending(); }

    void resume_simulation(const sc_time& duration);
    void pause_simulation(const string& reason);
    void force_quit();

    virtual void notify_step_complete(target& tgt) override;

    virtual void notify_breakpoint_hit(const breakpoint& bp) override;

    virtual void notify_watchpoint_read(const watchpoint& wp,
                                        const range& addr) override;

    virtual void notify_watchpoint_write(const watchpoint& wp,
                                         const range& addr,
                                         u64 newval) override;

public:
    vspserver() = delete;
    explicit vspserver(u16 port);
    virtual ~vspserver();

    void start();
    void cleanup();

    virtual void handle_connect(const char* peer) override;
    virtual void handle_disconnect() override;

    static vspserver* instance();
};

} // namespace debugging
} // namespace vcml

#endif
