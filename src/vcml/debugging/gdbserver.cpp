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

#include "vcml/debugging/gdbserver.h"
#include "vcml/module.h" // for commands

namespace vcml { namespace debugging {

    union gdb_u64 {
        u64 val;
        u8 ptr[8];

        gdb_u64(): val() {}
    };

    static inline int char2int(char c) {
        return ((c >= 'a') && (c <= 'f')) ? c - 'a' + 10 :
               ((c >= 'A') && (c <= 'F')) ? c - 'A' + 10 :
               ((c >= '0') && (c <= '9')) ? c - '0' :
               (c == '\0') ? 0 : -1;
    }

    static inline u64 str2int(const char* s, int n) {
        u64 val = 0;
        for (const char* c = s + n - 1; c >= s; c--) {
            val <<= 4;
            val |= char2int(*c);
        }
        return val;
    }

    static inline u8 char_unescape(const char*& s) {
        u8 result = *s++;
        if (result == '}')
            result = *s++ ^ 0x20;
        return result;
    }

    void gdbserver::update_status(gdb_status status) {
        if (!is_connected())
            status = m_default;

        if (!sim_running())
            status = GDB_KILLED;

        if (m_status == status)
            return;

        switch (status) {
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
            if (m_status == GDB_STOPPED)
                resume();
            break;

        default:
            VCML_ERROR("illegal gdb status: %u", status);
        }

        m_status = status;
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
                                            const range& addr,
                                            u64 newval) {
        update_status(GDB_STOPPED);
    }

    const cpureg* gdbserver::lookup_cpureg(unsigned int gdbno) {
        auto it = m_allregs.find(gdbno);
        if (it == m_allregs.end())
            return nullptr;
        return it->second;
    }

    gdbserver::handler gdbserver::find_handler(const char* command) {
        if (!stl_contains(m_handler, command[0]))
            return &gdbserver::handle_unknown;
        return m_handler.at(command[0]);
    }

    string gdbserver::handle_unknown(const char* command) {
        return "";
    }

    string gdbserver::handle_step(const char* command) {
        update_status(GDB_STEPPING);
        while (sim_running() && is_stepping()) {
            int signal = 0;
            if ((signal = recv_signal(100))) {
                log_debug("received signal %d", signal);
                break;
            }
        }

        update_status(GDB_STOPPED);
        return mkstr("S%02x", GDBSIG_TRAP);
    }

    string gdbserver::handle_continue(const char* command) {
        update_status(GDB_RUNNING);
        while (sim_running() && is_running()) {
            int signal = 0;
            if ((signal = recv_signal(100))) {
                log_debug("received signal %d", signal);
                break;
            }
        }

        update_status(GDB_STOPPED);
        return mkstr("S%02x", GDBSIG_TRAP);
    }

    string gdbserver::handle_detach(const char* command) {
        disconnect();
        return "";
    }

    string gdbserver::handle_kill(const char* command) {
        update_status(GDB_KILLED);
        suspender::quit();
        return "";
    }

    string gdbserver::handle_query(const char* command) {
        if (strncmp(command, "qSupported", strlen("qSupported")) == 0) {
            string features = mkstr("PacketSize=%zx;", PACKET_SIZE);
            if (m_target_arch != nullptr)
                features += "qXfer:features:read+;";
            return features;
        }

        if (strncmp(command, "qAttached", strlen("qAttached")) == 0)
            return "1";
        if (strncmp(command, "qOffsets", strlen("qOffsets")) == 0)
            return "Text=0;Data=0;Bss=0";
        if (strncmp(command, "qRcmd", strlen("qRcmd")) == 0)
            return handle_rcmd(command);
        if (strncmp(command, "qXfer", strlen("qXfer")) == 0)
            return handle_xfer(command);

        return handle_unknown(command);
    }

    string gdbserver::handle_rcmd(const char* command) {
        module* mod = dynamic_cast<module*>(&m_target);
        if (mod == nullptr)
            return ERR_COMMAND;

        vector<string> args = split(command, ' ');
        string cmdname = args[0];
        args.erase(args.begin());

        stringstream ss;
        if (!mod->execute(cmdname, args, ss))
            return ERR_COMMAND;

        return ss.str();
    }

    string gdbserver::handle_xfer(const char* command) {
        vector<string> args = split(command, ':');
        if (args.size() != 5)
            return ERR_COMMAND;

        string object = args[1];
        string read   = args[2];
        string annex  = args[3];

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

            if (length > m_target_xml.length())
                length = m_target_xml.length();

            if (offset > m_target_xml.length() - length)
                return ERR_COMMAND;

            bool more = offset + length < m_target_xml.length();
            return (more ? "m" : "l") + m_target_xml.substr(offset, length);
        }

        return "";
    }

    string gdbserver::handle_reg_read(const char* command) {
        unsigned int regno;
        if (sscanf(command, "p%x", &regno) != 1) {
            log_warn("malformed command '%s'", command);
            return ERR_COMMAND;
        }

        const cpureg* reg = lookup_cpureg(regno);
        if (reg == nullptr || !reg->is_readable())
            return "xxxxxxxx"; // respond with "contents unknown"

        u64 val = reg->read();
        if (!m_target.is_host_endian())
            memswap(&val, reg->size);

        stringstream ss;
        ss << std::hex << std::setfill('0');
        for (u32 shift = 0; shift < reg->width(); shift += 8)
            ss << std::setw(2) << ((val >> shift) & 0xff);

        return ss.str();
    }

    string gdbserver::handle_reg_write(const char* command) {
        gdb_u64 val;
        unsigned int regno;

        if (sscanf(command, "P%x=", &regno) != 1) {
            log_warn("malformed command '%str'", command);
            return ERR_COMMAND;
        }

        const cpureg* reg = lookup_cpureg(regno);
        if (reg == nullptr) {
            log_warn("unknown register id: %u", regno);
            return "OK";
        }

        const char* str = strrchr(command, '=');
        if (str == nullptr || strlen(str + 1) != reg->size  * 2) {
            log_warn("malformed command '%s'", command);
            return ERR_COMMAND;
        }

        str++; // step beyond '='

        for (u64 i = 0; i < reg->size; i++, str+= 2) {
            if (sscanf(str, "%02hhx", val.ptr + i) != 1) {
                log_warn("error parsing register value near %s", str);
                return ERR_COMMAND;
            }
        }

        if (!m_target.is_host_endian())
            memswap(val.ptr, reg->size);

        reg->write(val.val);
        return "OK";
    }

    string gdbserver::handle_reg_read_all(const char* command) {
        stringstream ss;
        ss << std::hex << std::setfill('0');

        for (const cpureg* reg : m_cpuregs) {
            if (!reg->is_readable())
                continue;

            u64 val = reg->read();
            if (!m_target.is_host_endian())
                memswap(&val, reg->size);

            for (u32 shift = 0; shift < reg->width(); shift += 8)
                ss << std::setw(2) << ((val >> shift) & 0xff);
        }

        return ss.str();
    }

    string gdbserver::handle_reg_write_all(const char* command) {
        const char* str = command + 1;
        for (const cpureg* reg : m_cpuregs) {
            if (!reg->is_writeable())
                continue;

            if (strlen(str) < reg->size * 2) {
                log_warn("malformed command '%s'", command);
                return ERR_COMMAND;
            }

            gdb_u64 val;
            for (u64 byte = 0; byte < reg->size; byte++, str += 2)
                sscanf(str, "%02hhx", val.ptr + byte);

            if (!m_target.is_host_endian())
                memswap(val.ptr, reg->size);

            reg->write(val.val);
        }

        return "OK";
    }

    string gdbserver::handle_mem_read(const char* command) {
        unsigned long long addr, size;
        if (sscanf(command, "m%llx,%llx", &addr, &size) != 2) {
            log_warn("malformed command '%s'", command);
            return ERR_COMMAND;
        }

        if (size > BUFFER_SIZE) {
            log_warn("too much data requested: %llu bytes", size);
            return ERR_PARAM;
        }

        stringstream ss;
        ss << std::hex << std::setfill('0');

        u8 buffer[BUFFER_SIZE];
        if (m_target.read_vmem_dbg(addr, buffer, size) != size)
            return ERR_UNKNOWN;

        for (unsigned int i = 0; i < size; i++)
            ss << std::setw(2) << (int)buffer[i];

        return ss.str();
    }

    string gdbserver::handle_mem_write(const char* command) {
        unsigned long long addr, size;
        if (sscanf(command, "M%llx,%llx", &addr, &size) != 2) {
            log_warn("malformed command '%s'", command);
            return ERR_COMMAND;
        }

        if (size > BUFFER_SIZE) {
            log_warn("too much data requested: %llu bytes", size);
            return ERR_PARAM;
        }

        const char* data = strchr(command, ':');
        if (data == NULL) {
            log_warn("malformed command '%s'", command);
            return ERR_COMMAND;
        }

        data++;

        u8 buffer[BUFFER_SIZE];
        for (unsigned int i = 0; i < size; i++)
            buffer[i] = str2int(data++, 2);

        if (m_target.write_vmem_dbg(addr, buffer, size) != size)
            return ERR_UNKNOWN;

        return "OK";
    }

    string gdbserver::handle_mem_write_bin(const char* command) {
        unsigned long long addr, size;
        if (sscanf(command, "X%llx,%llx:", &addr, &size) != 2) {
            log_warn("malformed command '%s'", command);
            return ERR_COMMAND;
        }

        if (size > BUFFER_SIZE) {
            log_warn("too much data requested: %llu bytes", size);
            return ERR_PARAM;
        }

        if (size == 0)
            return "OK"; // empty load to test if binary write is supported

        const char* data = strchr(command, ':');
        if (data == NULL) {
            log_warn("malformed command '%s'", command);
            return ERR_COMMAND;
        }

        data++;

        u8 buffer[BUFFER_SIZE];
        for (unsigned int i = 0; i < size; i++)
            buffer[i] = char_unescape(data);

        if (m_target.write_vmem_dbg(addr, buffer, size) != size)
            return ERR_UNKNOWN;

        return "OK";
    }

    string gdbserver::handle_breakpoint_set(const char* command) {
        unsigned long long type, addr, length;
        if (sscanf(command, "Z%llx,%llx,%llx", &type, &addr, &length) != 3) {
            log_warn("malformed command '%s'", command);
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

    string gdbserver::handle_breakpoint_delete(const char* command) {
        unsigned long long type, addr, length;
        if (sscanf(command, "z%llx,%llx,%llx", &type, &addr, &length) != 3) {
            log_warn("malformed command '%s'", command);
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

    string gdbserver::handle_exception(const char* command) {
        return mkstr("S%02u", GDBSIG_TRAP);
    }

    string gdbserver::handle_thread(const char* command) {
        return "OK";
    }

    string gdbserver::handle_vcont(const char* command) {
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
        m_allregs(),
        m_handler() {
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
                    m_allregs.insert({reg->regno, reg});
            } else {
                log_debug("gdb feature %s is not supported", feature.name);
            }
        }

        m_handler['q'] = &gdbserver::handle_query;

        m_handler['s'] = &gdbserver::handle_step;
        m_handler['c'] = &gdbserver::handle_continue;
        m_handler['D'] = &gdbserver::handle_detach;
        m_handler['k'] = &gdbserver::handle_kill;

        m_handler['p'] = &gdbserver::handle_reg_read;
        m_handler['P'] = &gdbserver::handle_reg_write;
        m_handler['g'] = &gdbserver::handle_reg_read_all;
        m_handler['G'] = &gdbserver::handle_reg_write_all;

        m_handler['m'] = &gdbserver::handle_mem_read;
        m_handler['M'] = &gdbserver::handle_mem_write;
        m_handler['X'] = &gdbserver::handle_mem_write_bin;

        m_handler['Z'] = &gdbserver::handle_breakpoint_set;
        m_handler['z'] = &gdbserver::handle_breakpoint_delete;

        m_handler['H'] = &gdbserver::handle_thread;
        m_handler['v'] = &gdbserver::handle_vcont;
        m_handler['?'] = &gdbserver::handle_exception;

        if (m_status == GDB_STOPPED)
            suspend();

        run_async();
    }

    gdbserver::~gdbserver() {
        shutdown();
    }

    string gdbserver::handle_command(const string& command) {
        try {
            handler func = find_handler(command.c_str());
            return (this->*func)(command.c_str());
        } catch (report& rep) {
            vcml::logger::log(rep);
            return ERR_INTERNAL;
        } catch (std::exception& ex) {
            log_warn("%s", ex.what());
            return ERR_INTERNAL;
        }
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

}}
