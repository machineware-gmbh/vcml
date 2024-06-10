/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/debugging/gdbserver.h"
#include "vcml/core/module.h" // for commands

namespace vcml {
namespace debugging {

bool gdbserver::parse_ids(const string& ids, int& pid, int& tid) const {
    if (m_support_processes) {
        if (sscanf(ids.c_str(), "p%x.%x", &pid, &tid) != 2)
            return false;
        return true;
    }

    pid = GDB_FIRST_TARGET;
    if (sscanf(ids.c_str(), "%x", &tid) != 1)
        return false;
    return true;
}

gdbserver::gdb_target* gdbserver::find_target(int pid, int tid) {
    if (tid == GDB_ALL_TARGETS || pid == GDB_ALL_TARGETS)
        return nullptr;
    if (pid == GDB_ANY_TARGET)
        pid = 1;
    if (tid == GDB_ANY_TARGET)
        return &m_targets[0];

    auto func = [tid, pid](gdb_target& t) {
        return t.tid == (u64)tid && t.pid == (u64)pid;
    };
    auto gtgt = find_if(m_targets.begin(), m_targets.end(), func);

    if (gtgt == m_targets.end())
        return nullptr;

    return &(*gtgt);
}

gdbserver::gdb_target* gdbserver::find_target(target& tgt) {
    for (auto& gtgt : m_targets)
        if (&gtgt.tgt == &tgt)
            return &gtgt;
    return nullptr;
}

string gdbserver::create_stop_reply() {
    stringstream ss;
    ss << mkstr("T%02xthread:%llx;", GDBSIG_TRAP, m_c_target->tid);

    if (m_hit_wp_addr && m_hit_wp_type != VCML_ACCESS_NONE) {
        const char wp_type = m_hit_wp_type == VCML_ACCESS_READ ? 'r' : 'a';
        ss << mkstr("%cwatch:%llx;", wp_type, m_hit_wp_addr->start);
    }
    m_hit_wp_addr = nullptr;
    m_hit_wp_type = VCML_ACCESS_NONE;

    return ss.str();
}

void gdbserver::cancel_singlestep() {
    for (auto& gtgt : m_targets)
        gtgt.tgt.cancel_singlestep(this);
}

void gdbserver::update_status(gdb_status status, gdb_target* gtgt,
                              const range* wp_addr, vcml_access wp_type) {
    lock_guard<mutex> guard(m_mtx);
    if (!is_connected())
        status = m_default;

    if (!sim_running())
        status = GDB_KILLED;

    if (m_status == status)
        return;

    gdb_status prev_status = m_status;

    switch ((m_status = status)) {
    case GDB_STOPPED:
        if (prev_status != GDB_STOPPED) {
            if (gtgt) {
                m_c_target = gtgt;
                m_g_target = gtgt;
            }
            m_hit_wp_addr = wp_addr;
            m_hit_wp_type = wp_type;
        }

        m_mtx.unlock();
        suspend(true);
        m_mtx.lock();
        break;

    case GDB_RUNNING:
    case GDB_STEPPING:
        resume();
        break;

    case GDB_KILLED:
        stop();
        disconnect();
        if (prev_status == GDB_STOPPED)
            resume();
        break;

    default:
        VCML_ERROR("illegal gdb status: %u", status);
    }
}

void gdbserver::notify_step_complete(target& tgt) {
    update_status(GDB_STOPPED, find_target(tgt));
}

void gdbserver::notify_breakpoint_hit(const breakpoint& bp) {
    update_status(GDB_STOPPED, find_target(bp.owner()));
}

void gdbserver::notify_watchpoint_read(const watchpoint& wp,
                                       const range& addr) {
    update_status(GDB_STOPPED, find_target(wp.owner()), &wp.address(),
                  VCML_ACCESS_READ);
}

void gdbserver::notify_watchpoint_write(const watchpoint& wp,
                                        const range& addr, u64 newval) {
    update_status(GDB_STOPPED, find_target(wp.owner()), &wp.address(),
                  VCML_ACCESS_WRITE);
}

bool gdbserver::check_suspension_point() {
    for (auto& gtgt : m_targets) {
        if (!gtgt.tgt.is_suspendable())
            return false;
    }

    return true;
}

string gdbserver::handle_unknown(const string& cmd) {
    return "";
}

string gdbserver::handle_step(const string& cmd) {
    if (!simulation_suspended()) {
        log_warn("simulation is not suspended");
        return ERR_INTERNAL;
    }

    if (!m_c_target) {
        log_warn("no target specified");
        return ERR_INTERNAL;
    }

    cancel_singlestep();

    for (auto& gtgt : m_targets)
        gtgt.tgt.set_running(true);

    m_c_target->tgt.request_singlestep(this);

    update_status(GDB_STEPPING);
    while (sim_running() && is_stepping()) {
        int signal = 0;
        if ((signal = recv_signal(1))) {
            log_debug("received signal %d", signal);
            break;
        }
    }

    update_status(GDB_STOPPED);

    if (sim_running()) {
        thctl_block();
        if (!simulation_suspended()) {
            log_warn("simulation is not suspended");
            return ERR_INTERNAL;
        }
    }

    return create_stop_reply();
}

string gdbserver::handle_continue(const string& cmd) {
    if (!simulation_suspended()) {
        log_warn("simulation is not suspended");
        return ERR_INTERNAL;
    }

    if (!m_c_target) {
        log_warn("no target specified");
        return ERR_INTERNAL;
    }

    cancel_singlestep();

    for (auto& gtgt : m_targets)
        gtgt.tgt.set_running(true);

    update_status(GDB_RUNNING);
    while (sim_running() && is_running()) {
        int signal = 0;
        if ((signal = recv_signal(1))) {
            log_debug("received signal %d", signal);
            break;
        }
    }

    update_status(GDB_STOPPED);

    if (sim_running()) {
        thctl_block();
        if (!simulation_suspended()) {
            log_warn("simulation is not suspended");
            return ERR_INTERNAL;
        }
    }

    return create_stop_reply();
}

string gdbserver::handle_detach(const string& cmd) {
    disconnect();
    return "";
}

string gdbserver::handle_kill(const string& cmd) {
    update_status(GDB_KILLED);
    suspender::quit();
    return "";
}

string gdbserver::handle_query(const string& cmd) {
    if (!simulation_suspended()) {
        log_warn("simulation is not suspended");
        return ERR_INTERNAL;
    }

    if (m_targets.size() == 0) {
        log_warn("no available target");
        return ERR_INTERNAL;
    }

    m_q_target = &m_targets[0];
    if (!m_q_target) {
        log_warn("no target specified");
        return ERR_INTERNAL;
    }

    if (starts_with(cmd, "qSupported")) {
        string features = mkstr("PacketSize=%zx;", PACKET_SIZE);
        if (m_q_target->arch != nullptr)
            features += "qXfer:features:read+;";
        features += "vContSupported+;";
        return features;
    }

    if (starts_with(cmd, "qAttached"))
        return "1";
    if (starts_with(cmd, "qOffsets"))
        return "Text=0;Data=0;Bss=0";
    if (starts_with(cmd, "qRcmd"))
        return handle_rcmd(cmd);
    if (starts_with(cmd, "qXfer"))
        return handle_xfer(cmd);

    if (starts_with(cmd, "qfThreadInfo")) {
        m_query_idx = 0;
        return handle_threadinfo(cmd);
    }
    if (starts_with(cmd, "qsThreadInfo"))
        return handle_threadinfo(cmd);

    if (starts_with(cmd, "qThreadExtraInfo,"))
        return handle_extra_threadinfo(cmd);
    if (starts_with(cmd, "qC"))
        return mkstr("QC%llx", m_q_target->tid);

    return handle_unknown(cmd);
}

string gdbserver::handle_rcmd(const string& cmd) {
    module* mod = dynamic_cast<module*>(&m_q_target->tgt);
    if (mod == nullptr)
        return ERR_COMMAND;

    vector<string> args = split(cmd, ' ');
    string cmdname = args[0];
    args.erase(args.begin());

    stringstream ss;
    if (!mod->execute(cmdname, args, ss))
        return ERR_COMMAND;

    return ss.str();
}

string gdbserver::handle_xfer(const string& cmd) {
    vector<string> args = split(cmd, ':');
    if (args.size() != 5)
        return ERR_COMMAND;

    string object = args[1];
    string read = args[2];
    string annex = args[3];

    if (read != "read")
        return ERR_COMMAND;

    size_t offset = 0, length = 0;
    if (sscanf(args[4].c_str(), "%zx,%zx", &offset, &length) != 2)
        return ERR_COMMAND;

    if (object == "features" && annex == "target.xml") {
        if (m_q_target->xml.empty()) {
            stringstream ss;
            m_q_target->arch->write_xml(m_q_target->tgt, ss);
            m_q_target->xml = ss.str();
        }

        if (offset > m_q_target->xml.length())
            return ERR_COMMAND;

        bool more = offset + length < m_q_target->xml.length();
        return (more ? "m" : "l") + m_q_target->xml.substr(offset, length);
    }

    return "";
}

string gdbserver::handle_threadinfo(const string& cmd) {
    if (m_query_idx >= m_targets.size())
        return "l";

    stringstream ss;
    ss << "m";

    for (; m_query_idx < m_targets.size(); m_query_idx++) {
        string s = mkstr("%llx,", m_targets[m_query_idx].tid);
        if (s.size() + ss.str().size() > BUFFER_SIZE) {
            m_query_idx--;
            break;
        }
        ss << s;
    }

    return ss.str();
}

string gdbserver::handle_extra_threadinfo(const string& cmd) {
    const char* str = cmd.c_str();
    str += 17;
    int pid = 0, tid = 0;
    if (!parse_ids(str, pid, tid)) {
        log_warn("malformed command %s", cmd.c_str());
        return ERR_COMMAND;
    }

    gdb_target* gtgt = find_target(pid, tid);
    if (!gtgt) {
        log_warn("unknown target ids %d.%d", pid, tid);
        return ERR_PARAM;
    }

    const char* info = gtgt->tgt.target_name();
    stringstream ss;
    for (size_t i = 0; i < strlen(info); i++)
        ss << mkstr("%02hhx", info[i]);

    return ss.str();
}

string gdbserver::handle_reg_read(const string& cmd) {
    if (!simulation_suspended()) {
        log_warn("simulation is not suspended");
        return ERR_INTERNAL;
    }

    unsigned int regno;
    if (sscanf(cmd.c_str(), "p%x", &regno) != 1) {
        log_warn("malformed command '%s'", cmd.c_str());
        return ERR_COMMAND;
    }

    if (!m_g_target) {
        log_warn("no specified target");
        return ERR_INTERNAL;
    }

    const cpureg* reg = m_g_target->tgt.find_cpureg(regno);
    if (reg == nullptr)
        return "xxxxxxxx"; // respond with "contents unknown"
    if (!reg->is_readable()) {
        return string(reg->total_size() * 2,
                      'x'); // respond with "contents unknown"
    }

    vector<u8> val(reg->total_size());
    if (!reg->read(val.data(), val.size()))
        return ERR_INTERNAL;

    stringstream ss;
    ss << std::hex << std::setfill('0');
    if (!m_g_target->tgt.is_host_endian()) {
        for (size_t i = 0; i < reg->total_size(); i += reg->size)
            memswap(val.data() + i, reg->size);
    }

    for (u8 v : val)
        ss << mkstr("%02hhx", v);

    return ss.str();
}

string gdbserver::handle_reg_write(const string& cmd) {
    if (!simulation_suspended()) {
        log_warn("simulation is not suspended");
        return ERR_INTERNAL;
    }

    unsigned int regno;
    if (sscanf(cmd.c_str(), "P%x=", &regno) != 1) {
        log_warn("malformed command '%str'", cmd.c_str());
        return ERR_COMMAND;
    }

    if (!m_g_target) {
        log_warn("no specified target");
        return ERR_INTERNAL;
    }

    const cpureg* reg = m_g_target->tgt.find_cpureg(regno);
    if (reg == nullptr) {
        log_warn("unknown register id: %u", regno);
        return "OK";
    }

    vector<u8> val(reg->total_size());
    const char* str = strrchr(cmd.c_str(), '=');
    if (str == nullptr || strlen(str + 1) != reg->total_size() * 2) {
        log_warn("malformed command '%s'", cmd.c_str());
        return ERR_COMMAND;
    }

    str++; // step beyond '='
    for (u64 byte = 0; byte < val.size(); byte++, str += 2) {
        if (sscanf(str, "%02hhx", val.data() + byte) != 1) {
            log_warn("error parsing register value near %s", str);
            return ERR_COMMAND;
        }
    }

    if (!m_g_target->tgt.is_host_endian()) {
        for (size_t i = 0; i < val.size(); i += reg->size)
            memswap(val.data() + i, reg->size);
    }

    if (!reg->write(val.data(), val.size()))
        return ERR_INTERNAL;

    return "OK";
}

string gdbserver::handle_reg_read_all(const string& cmd) {
    if (!simulation_suspended()) {
        log_warn("simulation is not suspended");
        return ERR_INTERNAL;
    }

    stringstream ss;
    ss << std::hex << std::setfill('0');

    if (!m_g_target) {
        log_warn("no specified target");
        return ERR_INTERNAL;
    }

    for (const cpureg* reg : m_g_target->cpuregs) {
        if (!reg->is_readable())
            continue;

        vector<u8> val(reg->total_size());
        if (!reg->read(val.data(), val.size()))
            return ERR_INTERNAL;

        if (!m_g_target->tgt.is_host_endian()) {
            for (size_t i = 0; i < val.size(); i += reg->size)
                memswap(val.data() + i, reg->size);
        }

        for (u8 v : val)
            ss << mkstr("%02hhx", v);
    }

    return ss.str();
}

string gdbserver::handle_reg_write_all(const string& cmd) {
    if (!simulation_suspended()) {
        log_warn("simulation is not suspended");
        return ERR_INTERNAL;
    }

    if (!m_g_target) {
        log_warn("no specified target");
        return ERR_INTERNAL;
    }

    const char* str = cmd.c_str() + 1;
    for (const cpureg* reg : m_g_target->cpuregs) {
        if (!reg->is_writeable())
            continue;

        if (strlen(str) < reg->total_size() * 2) {
            log_warn("malformed command '%s'", cmd.c_str());
            return ERR_COMMAND;
        }

        vector<u8> val(reg->total_size());
        for (u64 byte = 0; byte < val.size(); byte++, str += 2)
            sscanf(str, "%02hhx", val.data() + byte);

        if (!m_g_target->tgt.is_host_endian()) {
            for (size_t i = 0; i < val.size(); i += reg->size)
                memswap(val.data() + i, reg->size);
        }

        if (!reg->write(val.data(), val.size()))
            return ERR_INTERNAL;
    }

    return "OK";
}

string gdbserver::handle_mem_read(const string& cmd) {
    if (!simulation_suspended()) {
        log_warn("simulation is not suspended");
        return ERR_INTERNAL;
    }

    unsigned long long addr = 0, size = 0;
    if (sscanf(cmd.c_str(), "m%llx,%llx", &addr, &size) != 2) {
        log_warn("malformed command '%s'", cmd.c_str());
        return ERR_COMMAND;
    }

    if (size == 0)
        return "OK";

    if (size > BUFFER_SIZE) {
        log_warn("too much data requested: %llu bytes", size);
        return ERR_PARAM;
    }

    vector<u8> buffer;
    buffer.resize(size);

    if (!m_g_target) {
        log_warn("no specified target");
        return ERR_INTERNAL;
    }

    stringstream ss;
    ss << std::hex << std::setfill('0');
    if (m_g_target->tgt.read_vmem_dbg(addr, buffer.data(), size) != size)
        log_debug("failed to read 0x%llx..0x%llx", addr, addr + size - 1);

    for (unsigned long long i = 0; i < size; i++)
        ss << std::setw(2) << (int)buffer[i];

    return ss.str();
}

string gdbserver::handle_mem_write(const string& cmd) {
    if (!simulation_suspended()) {
        log_warn("simulation is not suspended");
        return ERR_INTERNAL;
    }

    unsigned long long addr = 0, size = 0;
    if (sscanf(cmd.c_str(), "M%llx,%llx", &addr, &size) != 2) {
        log_warn("malformed command '%s'", cmd.c_str());
        return ERR_COMMAND;
    }

    if (size == 0)
        return "OK";

    if (size > BUFFER_SIZE) {
        log_warn("too much data requested: %llu bytes", size);
        return ERR_PARAM;
    }

    const char* data = strchr(cmd.c_str(), ':');
    if (data == nullptr) {
        log_warn("malformed command '%s'", cmd.c_str());
        return ERR_COMMAND;
    }

    data++;

    vector<u8> buffer;
    buffer.resize(size);
    for (unsigned long long i = 0; i < size; i++) {
        buffer[i] = from_hex_ascii(data[2 * i + 0]) << 4 |
                    from_hex_ascii(data[2 * i + 1]);
    }

    if (!m_g_target) {
        log_warn("no specified target");
        return ERR_INTERNAL;
    }

    if (m_g_target->tgt.write_vmem_dbg(addr, buffer.data(), size) != size)
        return ERR_UNKNOWN;

    return "OK";
}

string gdbserver::handle_mem_write_bin(const string& cmd) {
    if (!simulation_suspended()) {
        log_warn("simulation is not suspended");
        return ERR_INTERNAL;
    }

    unsigned long long addr = 0, size = 0;
    if (sscanf(cmd.c_str(), "X%llx,%llx:", &addr, &size) != 2) {
        log_warn("malformed command '%s'", cmd.c_str());
        return ERR_COMMAND;
    }

    if (size == 0)
        return "OK";

    if (size > PACKET_SIZE) {
        log_warn("too much data requested: %llu bytes", size);
        return ERR_PARAM;
    }

    const char* data = strchr(cmd.c_str(), ':');
    if (data == nullptr) {
        log_warn("malformed command '%s'", cmd.c_str());
        return ERR_COMMAND;
    }

    data++;

    size_t len = cmd.c_str() + cmd.length() - data;
    if (len != size) {
        log_warn("mem_write_bin expected %llu bytes, got %zu", size, len);
        return ERR_COMMAND;
    }

    if (!m_g_target) {
        log_warn("no specified target");
        return ERR_INTERNAL;
    }

    if (m_g_target->tgt.write_vmem_dbg(addr, data, size) != size)
        return ERR_UNKNOWN;

    return "OK";
}

string gdbserver::handle_breakpoint_set(const string& cmd) {
    if (!simulation_suspended()) {
        log_warn("simulation is not suspended");
        return ERR_INTERNAL;
    }

    unsigned long long type, addr, length;
    if (sscanf(cmd.c_str(), "Z%llx,%llx,%llx", &type, &addr, &length) != 3) {
        log_warn("malformed command '%s'", cmd.c_str());
        return ERR_COMMAND;
    }

    const range wp(addr, addr + length - 1);
    switch (type) {
    case GDB_BREAKPOINT_SW:
    case GDB_BREAKPOINT_HW:
        for (auto& gtgt : m_targets) {
            if (!gtgt.tgt.insert_breakpoint(addr, this))
                return ERR_INTERNAL;
        }
        break;

    case GDB_WATCHPOINT_WRITE:
        for (auto& gtgt : m_targets) {
            if (!gtgt.tgt.insert_watchpoint(wp, VCML_ACCESS_WRITE, this))
                return ERR_INTERNAL;
        }
        break;

    case GDB_WATCHPOINT_READ:
        for (auto& gtgt : m_targets) {
            if (!gtgt.tgt.insert_watchpoint(wp, VCML_ACCESS_READ, this))
                return ERR_INTERNAL;
        }
        break;

    case GDB_WATCHPOINT_ACCESS:
        for (auto& gtgt : m_targets) {
            if (!gtgt.tgt.insert_watchpoint(wp, VCML_ACCESS_READ_WRITE, this))
                return ERR_INTERNAL;
        }
        break;

    default:
        log_warn("unknown breakpoint type %llu", type);
        return ERR_COMMAND;
    }

    return "OK";
}

string gdbserver::handle_breakpoint_delete(const string& cmd) {
    if (!simulation_suspended()) {
        log_warn("simulation is not suspended");
        return ERR_INTERNAL;
    }

    unsigned long long type, addr, length;
    if (sscanf(cmd.c_str(), "z%llx,%llx,%llx", &type, &addr, &length) != 3) {
        log_warn("malformed command '%s'", cmd.c_str());
        return ERR_COMMAND;
    }

    const range wp(addr, addr + length - 1);
    switch (type) {
    case GDB_BREAKPOINT_SW:
    case GDB_BREAKPOINT_HW:
        for (auto& gtgt : m_targets) {
            if (!gtgt.tgt.remove_breakpoint(addr, this))
                return ERR_INTERNAL;
        }
        break;

    case GDB_WATCHPOINT_WRITE:
        for (auto& gtgt : m_targets) {
            if (!gtgt.tgt.remove_watchpoint(wp, VCML_ACCESS_WRITE, this))
                return ERR_INTERNAL;
        }
        break;

    case GDB_WATCHPOINT_READ:
        for (auto& gtgt : m_targets) {
            if (!gtgt.tgt.remove_watchpoint(wp, VCML_ACCESS_READ, this))
                return ERR_INTERNAL;
        }
        break;

    case GDB_WATCHPOINT_ACCESS:
        for (auto& gtgt : m_targets) {
            if (!gtgt.tgt.remove_watchpoint(wp, VCML_ACCESS_READ_WRITE, this))
                return ERR_INTERNAL;
        }
        break;

    default:
        log_warn("unknown breakpoint type %llu", type);
        return ERR_COMMAND;
    }

    return "OK";
}

string gdbserver::handle_exception(const string& cmd) {
    if (!simulation_suspended()) {
        log_warn("simulation is not suspended");
        return ERR_INTERNAL;
    }

    if (!m_c_target) {
        log_warn("no target specified");
        return ERR_INTERNAL;
    }

    return mkstr("T%02uthread:%llx;", GDBSIG_TRAP, m_c_target->tid);
}

string gdbserver::handle_thread(const string& cmd) {
    const char* str = cmd.c_str();
    str += 2;
    int pid = 0, tid = 0;

    bool is_c = starts_with(cmd, "Hc");
    bool is_g = starts_with(cmd, "Hg");
    if (!is_c && !is_g) {
        log_warn("malformed command %s", cmd.c_str());
        return ERR_COMMAND;
    }

    if (!parse_ids(str, pid, tid)) {
        log_warn("malformed command %s", cmd.c_str());
        return ERR_COMMAND;
    }

    if (pid != GDB_ALL_TARGETS && tid != GDB_ALL_TARGETS) {
        auto gtgt = find_target(pid, tid);
        if (!gtgt) {
            log_warn("unknown target ids %d.%d", pid, tid);
            return ERR_PARAM;
        }

        if (is_c)
            m_c_target = gtgt;
        else
            m_g_target = gtgt;
    }

    return "OK";
}

string gdbserver::handle_thread_alive(const string& cmd) {
    const char* str = cmd.c_str();
    str++;

    int pid = 0, tid = 0;
    if (!parse_ids(str, pid, tid)) {
        log_warn("malformed command %s", cmd.c_str());
        return ERR_COMMAND;
    }

    if (!find_target(pid, tid)) {
        log_warn("target %d.%d is dead", pid, tid);
        return ERR_PARAM;
    }

    return "OK";
}

string gdbserver::handle_vcont(const string& cmd) {
    if (!simulation_suspended()) {
        log_warn("simulation is not suspended");
        return ERR_INTERNAL;
    }

    if (!starts_with(cmd, "vCont"))
        return "";

    if (cmd == "vCont?")
        return "s;S;c;C";

    vector<string> args = split(cmd, ';');
    if (args.size() <= 1) {
        log_warn("malformed command %s", cmd.c_str());
        return ERR_COMMAND;
    }
    args.erase(args.begin());

    vector<gdb_target*> m_unused_targets(m_targets.size());
    for (size_t i = 0; i < m_targets.size(); i++)
        m_unused_targets[i] = &m_targets[i];

    cancel_singlestep();

    gdb_status stat = GDB_RUNNING;
    for (auto& a : args) {
        int pid = 0, tid = 0;
        // Ignore signals for "C" and "S"
        if (starts_with(a, "c") || starts_with(a, "C")) {
            if (contains(a, ":")) {
                auto s = split(a, ':');
                if (!parse_ids(s[1], pid, tid)) {
                    log_warn("malformed command %s", cmd.c_str());
                    return ERR_COMMAND;
                }

                if (tid == GDB_ALL_TARGETS)
                    goto continue_all;

                auto gtgt = find_target(pid, tid);
                if (!gtgt) {
                    log_warn("unknown target ids %d.%d", pid, tid);
                    return ERR_PARAM;
                }

                if (!stl_contains(m_unused_targets, gtgt))
                    continue;

                gtgt->tgt.set_running(true);

                stl_remove(m_unused_targets, gtgt);
            } else {
            continue_all:
                for (auto gtgt : m_unused_targets)
                    gtgt->tgt.set_running(true);

                m_unused_targets.clear();
                break;
            }
        } else if (starts_with(a, "s") || starts_with(a, "S")) {
            if (contains(a, ":")) {
                stat = GDB_STEPPING;

                auto s = split(a, ':');
                if (!parse_ids(s[1], pid, tid)) {
                    log_warn("malformed command %s", cmd.c_str());
                    return ERR_COMMAND;
                }

                if (tid == GDB_ALL_TARGETS)
                    goto step_all;

                auto gtgt = find_target(pid, tid);
                if (!gtgt) {
                    log_warn("unknown target ids %d.%d", pid, tid);
                    return ERR_PARAM;
                }

                if (!stl_contains(m_unused_targets, gtgt))
                    continue;

                gtgt->tgt.set_running(true);
                gtgt->tgt.request_singlestep(this);
                stl_remove(m_unused_targets, gtgt);
            } else {
            step_all:
                for (auto gtgt : m_unused_targets) {
                    gtgt->tgt.set_running(true);
                    gtgt->tgt.request_singlestep(this);
                }

                m_unused_targets.clear();
                break;
            }
        } else {
            log_warn("malformed command %s", cmd.c_str());
            return ERR_COMMAND;
        }
    }

    for (auto gtgt : m_unused_targets)
        gtgt->tgt.set_running(false);

    update_status(stat);
    while (sim_running() && (is_stepping() || is_running())) {
        int signal = 0;
        if ((signal = recv_signal(1))) {
            log_debug("received signal %d", signal);
            break;
        }
    }

    update_status(GDB_STOPPED);

    if (sim_running()) {
        thctl_block();
        if (!simulation_suspended()) {
            log_warn("simulation is not suspended");
            return ERR_INTERNAL;
        }
    }

    return create_stop_reply();
}

gdbserver::gdbserver(u16 port, const vector<target*>& stubs,
                     gdb_status status):
    rspserver(port),
    subscriber(),
    suspender(mkstr("gdbserver_%hu", port)),
    m_targets(),
    m_c_target(),
    m_g_target(),
    m_q_target(),
    m_status(status),
    m_default(status),
    m_support_processes(false),
    m_query_idx(0),
    m_next_tid(1),
    m_hit_wp_addr(),
    m_hit_wp_type(VCML_ACCESS_NONE),
    m_cpuregs(),
    m_mtx() {
    if (stubs.size() == 0)
        VCML_ERROR("at least one target must be provided at construction");

    for (auto tgt : stubs)
        add_target(tgt);

    m_c_target = &m_targets[0];
    m_g_target = &m_targets[0];
    m_q_target = &m_targets[0];

    register_handler("q", &gdbserver::handle_query);

    register_handler("s", &gdbserver::handle_step);
    register_handler("c", &gdbserver::handle_continue);
    register_handler("D", &gdbserver::handle_detach);
    register_handler("k", &gdbserver::handle_kill);

    register_handler("p", &gdbserver::handle_reg_read);
    register_handler("P", &gdbserver::handle_reg_write);
    register_handler("g", &gdbserver::handle_reg_read_all);
    register_handler("G", &gdbserver::handle_reg_write_all);

    register_handler("m", &gdbserver::handle_mem_read);
    register_handler("M", &gdbserver::handle_mem_write);
    register_handler("X", &gdbserver::handle_mem_write_bin);

    register_handler("Z", &gdbserver::handle_breakpoint_set);
    register_handler("z", &gdbserver::handle_breakpoint_delete);

    register_handler("H", &gdbserver::handle_thread);
    register_handler("T", &gdbserver::handle_thread_alive);
    register_handler("v", &gdbserver::handle_vcont);
    register_handler("?", &gdbserver::handle_exception);

    if (m_status == GDB_STOPPED)
        suspend();

    run_async();
}

gdbserver::~gdbserver() {
    shutdown();
}

void gdbserver::handle_connect(const char* peer) {
    log_debug("gdb connected to %s", peer);
    update_status(GDB_STOPPED);

    if (sim_running()) {
        thctl_block();
        if (!simulation_suspended())
            log_warn("simulation is not suspended");
    }
}

void gdbserver::handle_disconnect() {
    log_debug("gdb disconnected");
    if (sim_running())
        update_status(m_default);
}

void gdbserver::add_target(target* tgt) {
    VCML_ERROR_ON(!tgt, "target cannot be a nullptr");

    const gdbarch* arch = gdbarch::lookup(tgt->arch());
    if (arch == nullptr)
        VCML_ERROR("architecture %s not supported", tgt->arch());

    vector<const cpureg*> cpuregs;
    if (!arch->collect_core_regs(*tgt, cpuregs))
        VCML_ERROR("%s does not support %s", tgt->target_name(), tgt->arch());

    for (const auto& feature : arch->features) {
        vector<const cpureg*> feature_cpuregs;
        if (feature.collect_regs(*tgt, feature_cpuregs))
            log_debug("gdb feature %s is supported by %s", feature.name,
                      tgt->target_name());
        else
            log_debug("gdb feature %s is not supported by %s", feature.name,
                      tgt->target_name());
    }

    m_targets.emplace_back(m_next_tid++, 1, arch, cpuregs, *tgt);
}

} // namespace debugging
} // namespace vcml
