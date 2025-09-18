/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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
    struct gdb_target {
        u64 tid;
        u64 pid;
        const gdbarch* arch;
        string xml;
        vector<const cpureg*> cpuregs;
        target& tgt;

        gdb_target(u64 t, u64 p, const gdbarch* a, vector<const cpureg*>& c,
                   target& tg):
            tid(t), pid(p), arch(a), xml(), cpuregs(c), tgt(tg) {}
    };

    vector<gdb_target> m_targets;
    gdb_target* m_c_target;
    gdb_target* m_g_target;
    gdb_target* m_q_target;
    atomic<gdb_status> m_status;
    gdb_status m_default;
    bool m_support_processes;
    size_t m_query_idx;
    u64 m_next_tid;
    const range* m_hit_wp_addr;
    vcml_access m_hit_wp_type;

    vector<const cpureg*> m_cpuregs;

    mutable mutex m_mtx;

    enum gdb_spec_ids {
        GDB_ALL_TARGETS = -1,
        GDB_ANY_TARGET = 0,
        GDB_FIRST_TARGET = 1
    };

    bool parse_ids(const string& ids, int& pid, int& tid) const;
    gdb_target* find_target(int pid, int tid);
    gdb_target* find_target(target& tgt);

    string create_stop_reply();

    void cancel_singlestep();

    void update_status(gdb_status status, gdb_target* gtgt = nullptr,
                       const range* wp_addr = nullptr,
                       vcml_access wp_type = VCML_ACCESS_NONE);

    virtual void notify_step_complete(target& tgt, const sc_time& t) override;

    virtual void notify_breakpoint_hit(const breakpoint& bp,
                                       const sc_time& t) override;

    virtual void notify_watchpoint_read(const watchpoint& wp,
                                        const range& addr,
                                        const sc_time& t) override;

    virtual void notify_watchpoint_write(const watchpoint& wp,
                                         const range& addr, const void* newval,
                                         const sc_time& t) override;

    virtual bool check_suspension_point() override;

    enum breakpoint_type {
        GDB_BREAKPOINT_SW = 0,
        GDB_BREAKPOINT_HW = 1,
        GDB_WATCHPOINT_WRITE = 2,
        GDB_WATCHPOINT_READ = 3,
        GDB_WATCHPOINT_ACCESS = 4,
    };

    string handle_unknown(int client, const string& command);

    string handle_query(int client, const string& command);
    string handle_rcmd(int client, const string& command);
    string handle_xfer(int client, const string& command);
    string handle_threadinfo(int client, const string& command);
    string handle_extra_threadinfo(int client, const string& command);
    string handle_step(int client, const string& command);
    string handle_continue(int client, const string& command);
    string handle_detach(int client, const string& command);
    string handle_kill(int client, const string& command);

    string handle_reg_read(int client, const string& command);
    string handle_reg_write(int client, const string& command);
    string handle_reg_read_all(int client, const string& command);
    string handle_reg_write_all(int client, const string& command);
    string handle_mem_read(int client, const string& command);
    string handle_mem_write(int client, const string& command);
    string handle_mem_write_bin(int client, const string& command);

    string handle_breakpoint_set(int client, const string& command);
    string handle_breakpoint_delete(int client, const string& command);

    string handle_exception(int client, const string& command);
    string handle_thread(int client, const string& command);
    string handle_thread_alive(int client, const string& command);
    string handle_vcont(int client, const string& command);

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
    gdbserver(const string& host, u16 port, const vector<target*>& stubs,
              gdb_status status = GDB_STOPPED);
    gdbserver(const string& host, u16 port, target& stub,
              gdb_status status = GDB_STOPPED):
        gdbserver(host, port, { &stub }, status) {}
    gdbserver(u16 port, const vector<target*>& stubs,
              gdb_status status = GDB_STOPPED):
        gdbserver("localhost", port, stubs, status) {}
    gdbserver(u16 port, target& stub, gdb_status status = GDB_STOPPED):
        gdbserver("localhost", port, { &stub }, status) {}
    virtual ~gdbserver();

    virtual void handle_connect(int client, const string& peer,
                                u16 port) override;
    virtual void handle_disconnect(int client) override;

    void add_target(target* tgt);
};

} // namespace debugging
} // namespace vcml

#endif
