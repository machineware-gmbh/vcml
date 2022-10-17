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

#ifndef VCML_GDBSERVER_H
#define VCML_GDBSERVER_H

#include "vcml/core/types.h"
#include "vcml/logging/logger.h"

#include "vcml/debugging/target.h"
#include "vcml/debugging/gdbarch.h"
#include "vcml/debugging/rspserver.h"
#include "vcml/debugging/suspender.h"
#include "vcml/debugging/subscriber.h"

namespace vcml {
namespace debugging {

enum gdb_status {
    GDB_STOPPED,
    GDB_STEPPING,
    GDB_RUNNING,
    GDB_KILLED,
};

enum gdb_signal {
    GDBSIG_TRAP = 5,
    GDBSIG_KILL = 9,
};

class gdbserver : public rspserver, private subscriber, private suspender
{
private:
    target& m_target;
    const gdbarch* m_target_arch;
    string m_target_xml;
    atomic<gdb_status> m_status;
    gdb_status m_default;

    vector<const cpureg*> m_cpuregs;
    unordered_map<u64, const cpureg*> m_allregs;

    void update_status(gdb_status status);

    virtual void notify_step_complete(target& tgt) override;

    virtual void notify_breakpoint_hit(const breakpoint& bp) override;

    virtual void notify_watchpoint_read(const watchpoint& wp,
                                        const range& addr) override;

    virtual void notify_watchpoint_write(const watchpoint& wp,
                                         const range& addr,
                                         u64 newval) override;

    const cpureg* lookup_cpureg(unsigned int gdbno);

    enum breakpoint_type {
        GDB_BREAKPOINT_SW = 0,
        GDB_BREAKPOINT_HW = 1,
        GDB_WATCHPOINT_WRITE = 2,
        GDB_WATCHPOINT_READ = 3,
        GDB_WATCHPOINT_ACCESS = 4
    };

    string handle_unknown(const string& command);

    string handle_query(const string& command);
    string handle_rcmd(const string& command);
    string handle_xfer(const string& command);
    string handle_step(const string& command);
    string handle_continue(const string& command);
    string handle_detach(const string& command);
    string handle_kill(const string& command);

    string handle_reg_read(const string& command);
    string handle_reg_write(const string& command);
    string handle_reg_read_all(const string& command);
    string handle_reg_write_all(const string& command);
    string handle_mem_read(const string& command);
    string handle_mem_write(const string& command);
    string handle_mem_write_bin(const string& command);

    string handle_breakpoint_set(const string& command);
    string handle_breakpoint_delete(const string& command);

    string handle_exception(const string& command);
    string handle_thread(const string& command);
    string handle_vcont(const string& command);

public:
    enum : size_t {
        PACKET_SIZE = 8 * MiB,
        BUFFER_SIZE = PACKET_SIZE / 2,
    };

    bool is_stopped() const { return m_status == GDB_STOPPED; }
    bool is_stepping() const { return m_status == GDB_STEPPING; }
    bool is_running() const { return m_status == GDB_RUNNING; }
    bool is_killed() const { return m_status == GDB_KILLED; }

    gdbserver() = delete;
    gdbserver(const gdbserver&) = delete;
    gdbserver(u16 port, target& stub, gdb_status status = GDB_STOPPED);
    virtual ~gdbserver();

    virtual void handle_connect(const char* peer) override;
    virtual void handle_disconnect() override;
};

} // namespace debugging
} // namespace vcml

#endif
