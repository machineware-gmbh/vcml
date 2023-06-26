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

void gdbserver::update_status(gdb_status status) {
    if (!is_connected())
        status = m_default;

    if (!sim_running())
        status = GDB_KILLED;

    if (m_status == status)
        return;

    gdb_status prev_status = m_status;

    switch ((m_status = status)) {
    case GDB_STOPPED:
        suspend();
        break;

    case GDB_RUNNING:
        resume();
        break;

    case GDB_STEPPING:
        m_target.request_singlestep(this);
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
    update_status(GDB_STOPPED);
}

void gdbserver::notify_breakpoint_hit(const breakpoint& bp) {
    update_status(GDB_STOPPED);
}

void gdbserver::notify_watchpoint_read(const watchpoint& wp,
                                       const range& addr) {
    update_status(GDB_STOPPED);
}

void gdbserver::notify_watchpoint_write(const watchpoint& wp,
                                        const range& addr, u64 newval) {
    update_status(GDB_STOPPED);
}

bool gdbserver::check_suspension_point() {
    return m_target.is_suspenable();
}

const cpureg* gdbserver::lookup_cpureg(unsigned int gdbno) {
    auto it = m_allregs.find(gdbno);
    if (it == m_allregs.end())
        return nullptr;
    return it->second;
}

string gdbserver::handle_unknown(const string& cmd) {
    return "";
}

string gdbserver::handle_step(const string& cmd) {
    update_status(GDB_STEPPING);
    while (sim_running() && is_stepping()) {
        int signal = 0;
        if ((signal = recv_signal(1))) {
            log_debug("received signal %d", signal);
            break;
        }
    }

    update_status(GDB_STOPPED);
    return mkstr("S%02x", GDBSIG_TRAP);
}

string gdbserver::handle_continue(const string& cmd) {
    update_status(GDB_RUNNING);
    while (sim_running() && is_running()) {
        int signal = 0;
        if ((signal = recv_signal(1))) {
            log_debug("received signal %d", signal);
            break;
        }
    }

    update_status(GDB_STOPPED);
    return mkstr("S%02x", GDBSIG_TRAP);
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
    if (starts_with(cmd, "qSupported")) {
        string features = mkstr("PacketSize=%zx;", PACKET_SIZE);
        if (m_target_arch != nullptr)
            features += "qXfer:features:read+;";
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

    return handle_unknown(cmd);
}

string gdbserver::handle_rcmd(const string& cmd) {
    module* mod = dynamic_cast<module*>(&m_target);
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
        if (m_target_xml.empty()) {
            stringstream ss;
            m_target_arch->write_xml(m_target, ss);
            m_target_xml = ss.str();
        }

        if (offset > m_target_xml.length())
            return ERR_COMMAND;

        bool more = offset + length < m_target_xml.length();
        return (more ? "m" : "l") + m_target_xml.substr(offset, length);
    }

    return "";
}

string gdbserver::handle_reg_read(const string& cmd) {
    unsigned int regno;
    if (sscanf(cmd.c_str(), "p%x", &regno) != 1) {
        log_warn("malformed command '%s'", cmd.c_str());
        return ERR_COMMAND;
    }

    const cpureg* reg = lookup_cpureg(regno);
    if (reg == nullptr || !reg->is_readable())
        return reg == nullptr ? "xxxxxxxx" : string(reg->total_size() * 2, 'x'); // respond with "contents unknown"

    vector<u8> val(reg->total_size());
    if (!reg->read(val.data(), reg->total_size()))
        return ERR_INTERNAL;

    stringstream ss;
    ss << std::hex << std::setfill('0');
    if (!m_target.is_host_endian()) {
        for (size_t i = 0; i < reg->total_size(); i += reg->size)
            memswap(val.data() + i, reg->size);
    }

    for (u8 v : val)
        ss << std::setw(2) << (u64)v;

    return ss.str();
}

string gdbserver::handle_reg_write(const string& cmd) {
    unsigned int regno;

    if (sscanf(cmd.c_str(), "P%x=", &regno) != 1) {
        log_warn("malformed command '%str'", cmd.c_str());
        return ERR_COMMAND;
    }

    const cpureg* reg = lookup_cpureg(regno);
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
    for (u64 byte = 0; byte < reg->total_size(); byte++, str += 2) {
        if (sscanf(str, "%02hhx", val.data() + byte) != 1) {
            log_warn("error parsing register value near %s", str);
            return ERR_COMMAND;
        }
    }

    if (!m_target.is_host_endian()) {
        for (size_t i = 0; i < reg->total_size(); i += reg->total_size())
            memswap(val.data() + i, reg->size);
    }

    if (!reg->write(val.data(), reg->total_size()))
        return ERR_INTERNAL;

    return "OK";
}

string gdbserver::handle_reg_read_all(const string& cmd) {
    stringstream ss;
    ss << std::hex << std::setfill('0');

    for (const cpureg* reg : m_cpuregs) {
        if (!reg->is_readable())
            continue;

        vector<u8> val(reg->total_size());
        if (!reg->read(val.data(), reg->total_size()))
            return ERR_INTERNAL;

        if (!m_target.is_host_endian()) {
            for (size_t i = 0; i < reg->total_size(); i += reg->size)
                memswap(val.data() + i, reg->size);
        }

        for (u8 v : val)
            ss << std::setw(2) << (u64)v;
    }

    return ss.str();
}

string gdbserver::handle_reg_write_all(const string& cmd) {
    const char* str = cmd.c_str() + 1;
    for (const cpureg* reg : m_cpuregs) {
        if (!reg->is_writeable())
            continue;

        if (strlen(str) < reg->total_size() * 2) {
            log_warn("malformed command '%s'", cmd.c_str());
            return ERR_COMMAND;
        }

        vector<u8> val(reg->total_size());
        for (u64 byte = 0; byte < reg->size; byte++, str += 2)
            sscanf(str, "%02hhx", val.data() + byte);

        if (!m_target.is_host_endian()) {
            for (size_t i = 0; i < reg->total_size(); i += reg->size)
                memswap(val.data() + i, reg->size);
        }

        if (!reg->write(val.data(), reg->total_size()))
            return ERR_INTERNAL;
    }

    return "OK";
}

string gdbserver::handle_mem_read(const string& cmd) {
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

    stringstream ss;
    ss << std::hex << std::setfill('0');
    if (m_target.read_vmem_dbg(addr, buffer.data(), size) != size)
        log_debug("failed to read 0x%llx..0x%llx", addr, addr + size - 1);

    for (unsigned long long i = 0; i < size; i++)
        ss << std::setw(2) << (int)buffer[i];

    return ss.str();
}

string gdbserver::handle_mem_write(const string& cmd) {
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

    if (m_target.write_vmem_dbg(addr, buffer.data(), size) != size)
        return ERR_UNKNOWN;

    return "OK";
}

string gdbserver::handle_mem_write_bin(const string& cmd) {
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

    if (m_target.write_vmem_dbg(addr, data, size) != size)
        return ERR_UNKNOWN;

    return "OK";
}

string gdbserver::handle_breakpoint_set(const string& cmd) {
    unsigned long long type, addr, length;
    if (sscanf(cmd.c_str(), "Z%llx,%llx,%llx", &type, &addr, &length) != 3) {
        log_warn("malformed command '%s'", cmd.c_str());
        return ERR_COMMAND;
    }

    const range wp(addr, addr + length - 1);
    switch (type) {
    case GDB_BREAKPOINT_SW:
    case GDB_BREAKPOINT_HW:
        if (!m_target.insert_breakpoint(addr, this))
            return ERR_INTERNAL;
        break;

    case GDB_WATCHPOINT_WRITE:
        if (!m_target.insert_watchpoint(wp, VCML_ACCESS_WRITE, this))
            return ERR_INTERNAL;
        break;

    case GDB_WATCHPOINT_READ:
        if (!m_target.insert_watchpoint(wp, VCML_ACCESS_READ, this))
            return ERR_INTERNAL;
        break;

    case GDB_WATCHPOINT_ACCESS:
        if (!m_target.insert_watchpoint(wp, VCML_ACCESS_READ_WRITE, this))
            return ERR_INTERNAL;
        break;

    default:
        log_warn("unknown breakpoint type %llu", type);
        return ERR_COMMAND;
    }

    return "OK";
}

string gdbserver::handle_breakpoint_delete(const string& cmd) {
    unsigned long long type, addr, length;
    if (sscanf(cmd.c_str(), "z%llx,%llx,%llx", &type, &addr, &length) != 3) {
        log_warn("malformed command '%s'", cmd.c_str());
        return ERR_COMMAND;
    }

    const range wp(addr, addr + length - 1);
    switch (type) {
    case GDB_BREAKPOINT_SW:
    case GDB_BREAKPOINT_HW:
        if (!m_target.remove_breakpoint(addr, this))
            return ERR_INTERNAL;
        break;

    case GDB_WATCHPOINT_WRITE:
        if (!m_target.remove_watchpoint(wp, VCML_ACCESS_WRITE, this))
            return ERR_INTERNAL;
        break;

    case GDB_WATCHPOINT_READ:
        if (!m_target.remove_watchpoint(wp, VCML_ACCESS_READ, this))
            return ERR_INTERNAL;
        break;

    case GDB_WATCHPOINT_ACCESS:
        if (!m_target.remove_watchpoint(wp, VCML_ACCESS_READ_WRITE, this))
            return ERR_INTERNAL;
        break;

    default:
        log_warn("unknown breakpoint type %llu", type);
        return ERR_COMMAND;
    }

    return "OK";
}

string gdbserver::handle_exception(const string& cmd) {
    return mkstr("S%02u", GDBSIG_TRAP);
}

string gdbserver::handle_thread(const string& cmd) {
    return "OK";
}

string gdbserver::handle_vcont(const string& cmd) {
    return "";
}

gdbserver::gdbserver(u16 port, target& stub, gdb_status status):
    rspserver(port),
    subscriber(),
    suspender(mkstr("gdbserver_%hu", port)),
    m_target(stub),
    m_target_arch(gdbarch::lookup(m_target.arch())),
    m_target_xml(),
    m_status(status),
    m_default(status),
    m_cpuregs(),
    m_allregs() {
    if (m_target_arch == nullptr)
        VCML_ERROR("architecture %s not supported", m_target.arch());

    if (!m_target_arch->collect_core_regs(m_target, m_cpuregs))
        VCML_ERROR("target does not support %s", m_target.arch());

    log_debug("gdb architecture %s is supported", m_target.arch());

    for (const auto& feature : m_target_arch->features) {
        vector<const cpureg*> cpuregs;
        if (feature.collect_regs(m_target, cpuregs)) {
            log_debug("gdb feature %s is supported", feature.name);
            for (const cpureg* reg : cpuregs)
                m_allregs.insert({ reg->regno, reg });
        } else {
            log_debug("gdb feature %s is not supported", feature.name);
        }
    }

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
}

void gdbserver::handle_disconnect() {
    log_debug("gdb disconnected");
    if (sim_running())
        update_status(m_default);
}

} // namespace debugging
} // namespace vcml
