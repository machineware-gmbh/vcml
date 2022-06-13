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

#ifndef VCML_VSPSERVER_H
#define VCML_VSPSERVER_H

#include "vcml/core/types.h"
#include "vcml/core/strings.h"
#include "vcml/core/report.h"
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

    string handle_none(const char* command);
    string handle_step(const char* command);
    string handle_cont(const char* command);
    string handle_list(const char* command);
    string handle_exec(const char* command);
    string handle_time(const char* command);
    string handle_rdgq(const char* command);
    string handle_wrgq(const char* command);
    string handle_geta(const char* command);
    string handle_seta(const char* command);
    string handle_quit(const char* command);
    string handle_vers(const char* command);
    string handle_mkbp(const char* command);
    string handle_rmbp(const char* command);

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
